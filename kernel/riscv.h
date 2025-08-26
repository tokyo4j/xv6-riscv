#ifndef __ASSEMBLER__

#define INLINE inline __attribute__((always_inline))

#define DEF_CSR(name) \
  static INLINE uint64 r_##name() { \
    uint64 x; \
    asm volatile("csrr %0, " #name : "=r" (x) ); \
    return x; \
  } \
  \
  static INLINE void w_##name(uint64 x) { \
    asm volatile("csrw " #name ", %0" : : "r" (x)); \
  }

DEF_CSR(mhartid)
DEF_CSR(mstatus)
DEF_CSR(mepc)
DEF_CSR(mcause)
DEF_CSR(sstatus)
DEF_CSR(sie)
DEF_CSR(mie)
DEF_CSR(sepc)
DEF_CSR(medeleg)
DEF_CSR(mideleg)
DEF_CSR(stvec)
DEF_CSR(stimecmp)
DEF_CSR(menvcfg)
DEF_CSR(pmpcfg0)
DEF_CSR(pmpaddr0)
DEF_CSR(satp)
DEF_CSR(scause)
DEF_CSR(stval)
DEF_CSR(mcounteren)
DEF_CSR(time)

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)

// Supervisor Status Register, sstatus

#define SSTATUS_SPP_MASK (3L << 8)
#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer

// Machine-mode Interrupt Enable
#define MIE_STIE (1L << 5)  // supervisor timer

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// enable device interrupts
static INLINE void
intr_on()
{
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static INLINE void
intr_off()
{
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static INLINE int
intr_get()
{
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

static INLINE uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}

// read and write tp, the thread pointer, which xv6 uses to hold
// this core's hartid (core number), the index into cpus[].
static INLINE uint64
r_tp()
{
  uint64 x;
  asm volatile("mv %0, tp" : "=r" (x) );
  return x;
}

static INLINE void 
w_tp(uint64 x)
{
  asm volatile("mv tp, %0" : : "r" (x));
}

static INLINE uint64
r_ra()
{
  uint64 x;
  asm volatile("mv %0, ra" : "=r" (x) );
  return x;
}

// flush the TLB.
static INLINE void
sfence_vma()
{
  // the zero, zero means flush all TLB entries.
  asm volatile("sfence.vma zero, zero");
}

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs

#endif // __ASSEMBLER__

#define PGSIZE 4096 // bytes per page
#define PGSHIFT 12  // bits of offset within a page

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // user can access

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// extract the three 9-bit page table indices from a virtual address.
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
