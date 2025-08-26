#include "../types.h"
#include "../param.h"
#include "../memlayout.h"
#include "../riscv.h"
#include "../defs.h"

static void timerinit();
extern void firmware_trap_entry();
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

struct mtte {
  uint64 va;
  uint8 id;
};

struct enclave_ctx {
  uint64 id;
  uint64 prev_sie;
  uint64 prev_pc;
};

// stack for M-mode
__attribute__ ((aligned (16))) char m_stack[4096 * NCPU];

// memory tracking table
#define pa_to_mtti(pa) ((pa - KERNBASE) / 4096)
static struct mtte mtt[pa_to_mtti(PHYSTOP)];
static struct enclave_ctx enclave_ctxs[20];
static uint64 next_enclave_id_for_ecreate = 1;
static struct enclave_ctx *running_ctxs[NCPU];

static int
validate_access(uint64 va, uint64 pa)
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

  struct enclave_ctx *my_ctx = running_ctxs[r_mhartid()];
  if (my_ctx) {
    if (mtte->id != my_ctx->id && r_mcause() == RISCV_EXCP_INST_SUCCESS) {
      PANIC("tried to run untrusted code from enclave va=%lx pa=%lx\n", va, pa);
    }
    if (mtte->id != 0 && mtte->va != (va & ~4095ull)) {
      PANIC("va mismatch in enclave: pa=%lx, mtte->va=%lx, va=%lx\n", pa, mtte->va, va);
    }
  } else {
    if (mtte->id != 0) {
      PANIC("tried to access enclave code/data from untrusted code");
    }
  }

  return 1;
}

static struct enclave_ctx *
get_enclave_ctx(uint64 id)
{
  for (int i = 0; i < 20; i++) {
    if (enclave_ctxs[i].id == id) {
      return &enclave_ctxs[i];
    }
  }
  PANIC("out of enclave ctx");
}

void
firmware_trap(uint64 regs[32])
{
  uint64 mcause = r_mcause();
  uint64 prev_mode = r_mstatus() & MSTATUS_MPP_MASK;

  switch (mcause) {
  case RISCV_EXCP_INST_ACCESS_FAULT:
    PANIC("fault_fetch");
  case RISCV_EXCP_INST_SUCCESS:
  case RISCV_EXCP_LOAD_SUCCESS:
  case RISCV_EXCP_STORE_SUCCESS: {
    uint64 va = r_mtval();
    uint64 pa = r_MPSEPA();

    uint64 mpsec = validate_access(va, pa) ?
                    CSR_MPSEC_ACCEPT : CSR_MPSEC_REJECT;
    w_MPSEC(mpsec);
    break;
  }
  case RISCV_EXCP_ILLEGAL_INST: {
    struct enclave_ctx *my_ctx = running_ctxs[r_mhartid()];
    w_mepc(r_mepc() + 4);

    switch (regs[10]) {
    case ECREATE: {
      ASSERT(prev_mode == MSTATUS_MPP_S);
      ASSERT(!my_ctx);
      struct enclave_ctx *ctx = get_enclave_ctx(0);
      ctx->id = next_enclave_id_for_ecreate++;
			printf("[ECREATE] id:%ld\n", ctx->id);
      regs[10] = ctx->id;
      break;
    }
    case EADD: {
      ASSERT(prev_mode == MSTATUS_MPP_S);
      ASSERT(!my_ctx);
      uint64 id = regs[11];
      uint64 epc_pa = regs[12];
      uint64 va = regs[13];
			printf("[EADD] id:%ld va:%lx->pa:%lx\n", id, va, epc_pa);
      struct enclave_ctx *ctx = get_enclave_ctx(id);
      if (!ctx) {
        PANIC("EADD: invalid id");
      }
      struct mtte *mtte = &mtt[pa_to_mtti(epc_pa)];
      if (mtte->id) {
        PANIC("MTT entry occupied\n");
      }
      mtte->id = id;
      mtte->va = va;
      sfence_vma();
      break;
    }
    case EENTER: {
      ASSERT(prev_mode == MSTATUS_MPP_U);
      ASSERT(!my_ctx);

      uint64 id = regs[11];
      uint64 pc = regs[12];
			printf("[EENTER] id:%ld pc:%lx\n", id, pc);
      struct enclave_ctx *next_ctx = get_enclave_ctx(id);
      ASSERT(next_ctx);

      running_ctxs[r_mhartid()] = next_ctx;
      next_ctx->prev_sie = r_sie();
      w_sie(0ull);
      next_ctx->prev_pc = r_mepc();
      w_mepc(pc);

      break;
    }
    case EEXIT: {
      ASSERT(prev_mode == MSTATUS_MPP_U);
      ASSERT(my_ctx);
			printf("[EEXIT]\n");

      w_sie(my_ctx->prev_sie);
      w_mepc(my_ctx->prev_pc);

      *my_ctx = (struct enclave_ctx){0};
      running_ctxs[r_mhartid()] = 0;
      break;
    }
    default:
      // normally unreached
      printf("delegating illegal instruction\n");
      w_stval(r_mtval());
      w_sepc(r_mepc());
      w_scause(r_mcause());
      w_mepc(r_stvec() & ~3ull);
      w_mstatus((r_mstatus() & ~MSTATUS_MPP_MASK) | MSTATUS_MPP_S);
      if (r_mstatus() & SSTATUS_SIE) {
        w_mstatus(r_mstatus() | SSTATUS_SPIE);
      } else {
        w_mstatus(r_mstatus() & ~SSTATUS_SPIE);
      }
      w_mstatus(r_mstatus() & ~SSTATUS_SIE);
      break;
    }
    break;
  }
  default: {
    printf("mcause=%lx\n", mcause);
    break;
  }
  }
}

// entry.S jumps here in machine mode on stack0.
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.

  uint64 mdeleg = 0xffff;
  mdeleg &= ~(1ul << RISCV_EXCP_ILLEGAL_INST);
  w_medeleg(mdeleg);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // enable page-success exception
  w_MPSEC(CSR_MPSEC_ENABLE);
  // set machine mode exception vector
  w_mtvec((uint64)firmware_trap_entry);
  // store M-mode stack in mscratch
  uint64 stack_addr = (uint64)m_stack + (r_mhartid() + 1) * 4096;
  w_mscratch(stack_addr);

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
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
