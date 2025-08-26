#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

uint64
sys_ecreate(void)
{
	uint64 id;
	asm volatile(
  	"li a0, %1;"
  	".word 0x00000000;"
		"mv %0, a0;"
  	:"=r"(id): "i"(ECREATE)
  	:"a0");
  return id;
}

uint64
sys_eadd(void)
{
  struct proc *p = myproc();
  uint64 enclave_id, va;
  argaddr(0, &enclave_id);
  argaddr(1, &va);
  if (va & 4095) {
    panic("eadd: unaligned addr");
  }

  uint64 pa = walkaddr(p->pagetable, va);
  if (!pa) {
    panic("eadd: kalloc() failed");
  }

	asm volatile(
  	"li a0, %0;"
  	"mv a1, %1;"
  	"mv a2, %2;"
  	"mv a3, %3;"
  	".word 0x00000000;"
  	:: "i"(EADD), "r"(enclave_id), "r"(pa), "r"(va)
  	:"a0", "a1", "a2", "a3");

  sfence_vma();

  return 0;
}
