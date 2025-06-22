#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();
void pse_entry();

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];
// stack for M-mode
__attribute__ ((aligned (16))) char m_stack[4096 * NCPU];

#define CSR_MPSEC 0xbc0 /* page-success exception control */
#define CSR_MPSEC_ENABLE 1
#define CSR_MPSEC_DISABLE 2
#define CSR_MPSEC_ACCEPT 4
#define CSR_MPSEC_REJECT 8
#define CSR_MPSEPA 0xbc1 /* page-success exception physical address */

// memory region inaccessible to S-mode
__attribute__ ((aligned (4096))) char enclave[4096] = "This is enclave";
uint64 enclave_va = PHYSTOP + 4096;

void
pse_handler(void)
{
  uint64 va, pa;
  asm volatile("csrr %0, mtval" : "=r"(va));
  asm volatile("csrr %0, %1" : "=r"(pa) : "i"(CSR_MPSEPA));
  if (va == enclave_va && pa == (uint64)enclave) {
    /* Reject accesses to the enclave based on its VA and PA */
    asm volatile("csrw %0, %1" :: "i"(CSR_MPSEC), "r"(CSR_MPSEC_REJECT));
  } else {
    asm volatile("csrw %0, %1" :: "i"(CSR_MPSEC), "r"(CSR_MPSEC_ACCEPT));
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
  w_medeleg(0xffff);
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
  asm volatile("csrw %0, %1" :: "i"(CSR_MPSEC), "r"(CSR_MPSEC_ENABLE));
  // set machine mode exception vector
  asm volatile("csrw mtvec, %0" :: "r" (pse_entry));
  // store M-mode stack in mscratch
  uint64 stack_addr = (uint64)m_stack + (r_mhartid() + 1) * 4096;
  asm volatile("csrw mscratch, %0" :: "r" (stack_addr));

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}

// ask each hart to generate timer interrupts.
void
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
