#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/riscv.h"
#include "user/user.h"

uint64 fib_result;
extern char enclave_start[], enclave_end[];

__attribute__((section(".enclave.data")))
static uint64 fib_nums[32] = {1, 1};
__attribute__((section(".enclave.bss")))
static __attribute__((aligned(4096))) char enclave_stack[4096];

static void
__attribute__((section(".enclave.text")))
enclave_main(void)
{
  for (int i = 2; i < 32; i++) {
    fib_nums[i] = fib_nums[i-2] + fib_nums[i-1];
  }
  fib_result = fib_nums[31];
  return;
}

static void
__attribute__((section(".enclave.text.entry"), naked))
enclave_entry(void)
{
  asm volatile(
    "mv s0, sp;"
    "mv sp, %0;" // set enclave stack
    "jalr %1;" // goto main
    "mv sp, s0;" // restore stack
    "li a0, %2;"
    ".word 0x00000000" // EEXIT
    :: "r"(enclave_stack + 4096), "r"(enclave_main), "i"(EEXIT));
}

int
main(int argc, char *argv[])
{
  int id = ecreate();
  printf("start=%lx, end=%lx\n", (uint64)enclave_start, (uint64)enclave_end);

  uint64 enclave_size = enclave_end - enclave_start;

  printf("enclave_size=%lx\n", enclave_size);
  for (uint64 i = 0; i < enclave_size; i += 4096) {
    eadd(id, enclave_start + i);
  }

  asm volatile(
    "li a0, %0;"
    "mv a1, %1;"
    "mv a2, %2;"
    ".word 0x00000000;"
    :: "i"(EENTER), "r"(id), "r"(enclave_entry)
    :"a0", "a1", "a2");

  printf("result=%lx\n", fib_result);

  while(1);

  exit(0);
}
