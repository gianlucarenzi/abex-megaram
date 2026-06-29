/*
 * stm32h5e5zjx_flash.ld — Linker script for STM32H5E5ZJ
 *
 * Memory (from RM0492 / STM32CubeIDE auto-generated script):
 *   Flash : 4096 KB at 0x08000000
 *   RAM   : 1536 KB at 0x20000000  (contiguous SRAM1+SRAM2+SRAM3+SRAM4)
 *
 * Layout:
 *   RAM      256 KB  → stack / .data / .bss
 *   SRAM_EXP 1280 KB → expansion_ram[] (.sram NOLOAD, 1 MB used out of 1.28 MB)
 *
 * 256 + 1280 = 1536 KB (uses all internal SRAM).
 */

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);
_sstack = _estack - 0x800;   /* required by H5 startup file */

_Min_Heap_Size  = 0x400;
_Min_Stack_Size = 0x800;

MEMORY
{
    FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 4096K
    RAM      (xrw) : ORIGIN = 0x20000000, LENGTH = 256K
    SRAM_EXP (rw)  : ORIGIN = 0x20040000, LENGTH = 1280K
}

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    .ARM.extab :
    {
        . = ALIGN(4);
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } >FLASH

    .ARM :
    {
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
        . = ALIGN(4);
    } >FLASH

    .preinit_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .init_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .fini_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
        . = ALIGN(4);
    } >FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        *(.RamFunc)
        *(.RamFunc*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT>FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /* expansion_ram[] — 1 MB in SRAM_EXP; zeroed at runtime by main(). */
    .sram (NOLOAD) :
    {
        . = ALIGN(4);
        *(.sram)
        *(.sram*)
        . = ALIGN(4);
    } >SRAM_EXP

    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
