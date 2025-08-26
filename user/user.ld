OUTPUT_ARCH( "riscv" )

SECTIONS
{
 . = 0x0;
 
  .text : {
    *(.text .text.*)
  }

  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .eh_frame : {
       *(.eh_frame)
       *(.eh_frame.*)
   }

  . = ALIGN(0x1000);
  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)
  }

  .enclave : {
    . = ALIGN(4096);
    PROVIDE(enclave_start = .);
    *(.enclave.text.entry)
    *(.enclave.*)
    . = ALIGN(4096);
    PROVIDE(enclave_end = .);
  }

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
    . = ALIGN(16);
    *(.bss .bss.*)
  }

  PROVIDE(end = .);
}
