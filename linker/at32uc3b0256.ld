/* SPDX-License-Identifier: MIT
 * AT32UC3B0256 only: 32059L, sections 6.5, 7.1/7.2, 8.2.
 * Factory USB DFU application: AVR32784 section 6 (doc7745).
 * DO NOT overwrite the factory USB DFU bootloader.
 */
OUTPUT_FORMAT("elf32-avr32", "elf32-avr32", "elf32-avr32")
OUTPUT_ARCH(avr32:uc)
ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x80002000, LENGTH = 248K
    /* First word is the bootloader's ISPK warm-reset key. */
    SRAM  (rwx) : ORIGIN = 0x00000004, LENGTH = 32K - 4
}

/* Project policy, not a hardware requirement: reserve 1 KiB for the stack. */
__stack_size__ = 1024;
__stack_top__ = ORIGIN(SRAM) + LENGTH(SRAM);
__stack_bottom__ = __stack_top__ - __stack_size__;

SECTIONS
{
    .reset ORIGIN(FLASH) :
    {
        KEEP(*(.reset))
    } > FLASH

    /* Low nine base bits are zero for fixed slots and timer stub at 0x104.
     * INTC forms EVBA OR offset; all used offsets fit these cleared bits.
     */
    .exception_vectors ALIGN(512) :
    {
        KEEP(*(.exception_vectors))
    } > FLASH

    __system_timer_vector_offset = __system_timer_vector - __exception_vectors;
    __uart_vector_offset = __uart_vector - __exception_vectors;
    __usb_vector_offset = __usb_vector - __exception_vectors;

    .text ALIGN(4) :
    {
        *(.text .text.*)
        . = ALIGN(4);
    } > FLASH

    .rodata ALIGN(4) :
    {
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > FLASH

    .data ORIGIN(SRAM) :
    {
        __data_start__ = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end__ = .;
    } > SRAM AT > FLASH
    __data_load__ = LOADADDR(.data);
    __flash_end__ = LOADADDR(.data) + SIZEOF(.data);

    .bss ALIGN(4) (NOLOAD) :
    {
        __bss_start__ = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > SRAM

    .stack __stack_bottom__ (NOLOAD) :
    {
        KEEP(*(.stack))
        . = __stack_size__;
    } > SRAM

    /* Fail explicitly if later code assumes a runtime Stage 1 cannot provide. */
    .unsupported_runtime :
    {
        KEEP(*(.init .fini .ctors .ctors.* .dtors .dtors.*))
        KEEP(*(.preinit_array* .init_array* .fini_array*))
        *(.tdata .tdata.* .tbss .tbss.* .got .got.* .plt .plt.*)
    } > SRAM

    /DISCARD/ : { *(.comment .note.GNU-stack) }
}

ASSERT(_start == ORIGIN(FLASH), "Reset entry must be at the flash base")
ASSERT((__exception_vectors & 511) == 0, "EVBA must be 512-byte aligned")
ASSERT(__system_timer_vector_offset == 0x104, "Unexpected timer vector offset")
ASSERT((__exception_vectors | __system_timer_vector_offset) == __system_timer_vector,
       "Timer autovector must obey EVBA OR offset")
ASSERT(__uart_vector_offset == 0x108, "Unexpected UART vector offset")
ASSERT((__exception_vectors | __uart_vector_offset) == __uart_vector,
       "UART autovector must obey EVBA OR offset")
ASSERT((__stack_top__ & 7) == 0, "Stack must be 8-byte aligned")
ASSERT(__usb_vector_offset == 0x10c, "Unexpected USB vector offset")
ASSERT((__exception_vectors | __usb_vector_offset) == __usb_vector,
       "USB autovector must obey EVBA OR offset")
ASSERT(__bss_end__ <= __stack_bottom__, "SRAM data overlaps reserved stack")
ASSERT(__flash_end__ <= ORIGIN(FLASH) + LENGTH(FLASH), "Flash overflow")
ASSERT(SIZEOF(.unsupported_runtime) == 0, "Unsupported C++/TLS/PIC runtime")
