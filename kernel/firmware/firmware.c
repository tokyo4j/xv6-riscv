#include "../types.h"
#include "../param.h"
#include "../memlayout.h"
#include "../riscv.h"
#include "../defs.h"

struct cpu_ctx;

static void timerinit();
extern void world_enter(struct cpu_ctx *ctx);
extern char firmware_start[], firmware_end[];

#define PANIC(...) do { \
  extern int panicking; \
  panicking = 1; \
  printf(__VA_ARGS__); \
  while (1) {} \
} while (0)

#define ASSERT(stmt) do { \
  if (!(stmt)) {\
    extern int panicking; \
    panicking = 1; \
    printf(#stmt); \
    while (1) {} \
  }\
} while (0)

#define NULL (void *)0
#define true 1
#define false 0
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

struct mtte {
  uint64 va;
  uint8 id;
};

#define CTX_ID_INVALID 0
#define CTX_ID_NORMAL 1

struct cpu_ctx {
  struct {
    uint64 s[12]; // 0
    uint64 sp; // 12
    uint64 ra; // 13
  } firmware;
  struct {
    uint64 x[32]; // 14
    uint64 mstatus; // 46
    uint64 sip; // 47
    uint64 sie; // 48
    uint64 stvec; // 49
    uint64 sscratch; // 50
    uint64 sepc; // 51
    uint64 scause; // 52
    uint64 stval; // 53
    uint64 satp; // 54
    uint64 scounteren; // 55
    uint64 senvcfg; // 56
    uint64 vsstatus; // 57
    uint64 vsie; // 58
    uint64 vstvec; // 59
    uint64 vsscratch; // 60
    uint64 vsepc; // 61
    uint64 vscause; // 62
    uint64 vstval; // 63
    uint64 vsatp; // 64
    uint64 hvip; // 65
    uint64 mepc; // 66
  } world;
  uint64 id;
  struct cpu_ctx *prev_ctx;
};

// stack for M-mode
__attribute__ ((aligned (16))) char m_stack[4096 * NCPU];

// memory tracking table
#define pa_to_mtti(pa) ((pa - KERNBASE) / 4096)
static struct mtte mtt[pa_to_mtti(PHYSTOP)];
static struct cpu_ctx ctxs[20];
static uint64 next_tee_id_for_ecreate = CTX_ID_NORMAL + 1;

static int
validate_access(struct cpu_ctx *ctx, uint64 va, uint64 pa)
{
  if ((uint64)firmware_start <= pa && pa < (uint64)firmware_end) {
    PANIC("access to m-mode region");
    return 0;
  }

  /* I/O */
  if (pa < KERNBASE || pa >= PHYSTOP) {
    return 1;
  }

  struct mtte *mtte = &mtt[pa_to_mtti(pa)];

  if (ctx->id > CTX_ID_NORMAL) {
    if (mtte->id != ctx->id && r_mcause() == RISCV_EXCP_INST_SUCCESS) {
      PANIC("tried to run untrusted code from enclave va=%lx pa=%lx\n", va, pa);
    }
    if (mtte->id != 0 && mtte->va != (va & ~4095ull)) {
      PANIC("va mismatch in enclave: pa=%lx, mtte->va=%lx, va=%lx\n", pa, mtte->va, va);
    }
  } else {
    if (mtte->id > CTX_ID_NORMAL) {
      PANIC("tried to access enclave code/data from untrusted code");
    }
  }

  return 1;
}

static struct cpu_ctx *
get_cpu_ctx(uint64 id)
{
  for (int i = 0; i < ARRAY_SIZE(ctxs); i++) {
    if (ctxs[i].id == id) {
      return &ctxs[i];
    }
  }
  PANIC("out of enclave ctx");
}

static struct cpu_ctx *
handle_trap(struct cpu_ctx *ctx)
{
  uint64 mcause = r_mcause();
  uint64 prev_mode = ctx->world.mstatus & MSTATUS_MPP_MASK;

  switch (mcause) {
  case RISCV_EXCP_INST_ACCESS_FAULT:
    PANIC("fault_fetch");
  case RISCV_EXCP_INST_SUCCESS:
  case RISCV_EXCP_LOAD_SUCCESS:
  case RISCV_EXCP_STORE_SUCCESS: {
    uint64 va = r_mtval();
    uint64 pa = r_MPSEPA();
    uint64 mpsec = validate_access(ctx, va, pa) ?
                    CSR_MPSEC_ACCEPT : CSR_MPSEC_REJECT;
    w_MPSEC(mpsec);
    return NULL;
  }
  case RISCV_EXCP_ILLEGAL_INST: {
    switch (ctx->world.x[10]) {
    case ECREATE: {
      ASSERT(prev_mode == MSTATUS_MPP_S);
      ASSERT(ctx->id == CTX_ID_NORMAL);
      struct cpu_ctx *new_ctx = get_cpu_ctx(0);
      new_ctx->id = next_tee_id_for_ecreate++;
			printf("[ECREATE] id:%ld\n", new_ctx->id);
      ctx->world.mepc += 4;
      ctx->world.x[10] = new_ctx->id;
      return NULL;
    }
    case EADD: {
      ASSERT(prev_mode == MSTATUS_MPP_S);
      ASSERT(ctx->id == CTX_ID_NORMAL);
      uint64 id = ctx->world.x[11];
      uint64 epc_pa = ctx->world.x[12];
      uint64 va = ctx->world.x[13];
			printf("[EADD] id:%ld va:%lx->pa:%lx\n", id, va, epc_pa);
      struct cpu_ctx *enc_ctx = get_cpu_ctx(id);
      if (!enc_ctx) {
        PANIC("EADD: invalid id");
      }
      struct mtte *mtte = &mtt[pa_to_mtti(epc_pa)];
      if (mtte->id) {
        PANIC("MTT entry occupied\n");
      }
      mtte->id = id;
      mtte->va = va;
      sfence_vma();
      ctx->world.mepc += 4;
      return NULL;
    }
    case EENTER: {
      ASSERT(prev_mode == MSTATUS_MPP_U);
      ASSERT(ctx->id == CTX_ID_NORMAL);

      uint64 id = ctx->world.x[11];
      uint64 pc = ctx->world.x[12];
			printf("[EENTER] id:%ld pc:%lx\n", id, pc);
      struct cpu_ctx *next_ctx = get_cpu_ctx(id);
      ASSERT(next_ctx);

      next_ctx->world.sie = 0ull; // disable interrupts for now
      next_ctx->world.mepc = pc;
      next_ctx->world.satp = ctx->world.satp; // share address space
      ctx->world.mepc += 4;
      next_ctx->prev_ctx = ctx;

      return next_ctx;
    }
    case EEXIT: {
      ASSERT(prev_mode == MSTATUS_MPP_U);
      ASSERT(ctx->id > CTX_ID_NORMAL);
      ASSERT(ctx->prev_ctx);
			printf("[EEXIT]\n");
      struct cpu_ctx *prev_ctx = ctx->prev_ctx;
      ctx->prev_ctx = NULL;
      return prev_ctx;
    }
    default:
      // normally unreached
      printf("delegating illegal instruction\n");
      ctx->world.stval = r_mtval();
      ctx->world.sepc = r_mepc();
      ctx->world.scause = r_mcause();
      ctx->world.mepc = r_stvec() & ~3ull;

      ctx->world.mstatus &= ~MSTATUS_MPP_MASK;
      if (prev_mode == MSTATUS_MPP_S) {
        ctx->world.mstatus |= MSTATUS_MPP_S;
      }
      ctx->world.mstatus &= ~SSTATUS_SPIE;
      if (ctx->world.mstatus & SSTATUS_SIE) {
        ctx->world.mstatus |= SSTATUS_SPIE;
      }
      ctx->world.mstatus &= ~SSTATUS_SIE;
      return NULL;
    }
  }
  default: {
    printf("mcause=%lx\n", mcause);
    return NULL;
  }
  }
}

// entry.S jumps here in machine mode on stack0.
void
start()
{
  struct cpu_ctx *ctx = get_cpu_ctx(CTX_ID_INVALID);
  ctx->id = CTX_ID_NORMAL;

  // set M Previous Privilege mode to Supervisor, for mret.
  ctx->world.mstatus &= ~MSTATUS_MPP_MASK;
  ctx->world.mstatus |= MSTATUS_MPP_S;

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  ctx->world.mepc = (uint64)main;

  // disable paging for now.
  ctx->world.satp = 0;

  // delegate all interrupts and exceptions (except for illegal instruction)
  // to supervisor mode.
  uint64 mdeleg = 0xffff;
  mdeleg &= ~(1ul << RISCV_EXCP_ILLEGAL_INST);
  w_medeleg(mdeleg);
  w_mideleg(0xffff);
  ctx->world.sie = r_sie() | SIE_SEIE | SIE_STIE;

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  ctx->world.x[4] = r_mhartid(); // tp = x4

  // enable page-success exception
  w_MPSEC(CSR_MPSEC_ENABLE);

  // set up kernel stack
  extern char stack0[4096 * NCPU];
  ctx->world.x[2] = (uint64)(stack0 + 4096 * (r_mhartid() + 1));

  while (1) {
    world_enter(ctx);
    struct cpu_ctx *next_ctx = handle_trap(ctx);
    if (next_ctx) {
      ctx = next_ctx;
    }
  }
}

// ask each hart to generate timer interrupts.
static void
timerinit()
{
  // enable supervisor-mode timer interrupts.
  w_mie(r_mie() | MIE_STIE);
  
  // enable the sstc extension (i.e. stimecmp).
  w_menvcfg(r_menvcfg() | (1L << 63)); 
  
  // allow supervisor to use stimecmp and time.
  w_mcounteren(r_mcounteren() | 2);
  
  // ask for the very first timer interrupt.
  w_stimecmp(r_time() + 1000000);
}
