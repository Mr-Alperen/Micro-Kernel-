ENTRY(_start)

SECTIONS
{
  . = 0x00100000; /* load at 1MiB */

  .text : { *(.multiboot) *(.text) }
  .rodata : { *(.rodata) }
  .data : { *(.data) }
  .bss  : { *(.bss) }
  
  /* Çekirdeğin bellekteki sonunu işaretleyen sembol */
  end = .;
}