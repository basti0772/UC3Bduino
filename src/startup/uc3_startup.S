/* SPDX-License-Identifier: MIT
 * Original AVR32 UC startup. Hardware references: docs/HARDWARE.md.
 * The linker owns the device-specific memory addresses.
 */

.equ UC_SR_GM_BIT, 16
.equ UC_SR_EM_BIT, 21
.equ UC_SYSREG_EVBA, 4

.section .reset, "ax", @progbits
.balign 4
.global _start
.type _start, @function
_start:
    ssrf UC_SR_GM_BIT
    /* 32059L, section 31.2.4 (Rev B): two NOPs after masking interrupts. */
    nop
    nop
    lda.w sp, __stack_top__
    lda.w r8, __exception_vectors
    mtsr UC_SYSREG_EVBA, r8
    /* Enable normal exceptions only after SP and EVBA are initialized. */
    csrf UC_SR_EM_BIT

    /* Word-aligned bounds and sizes are guaranteed by the linker. */
    lda.w r8, __data_load__
    lda.w r9, __data_start__
    lda.w r10, __data_end__
.Lcopy_data:
    cp.w r9, r10
    breq .Lprepare_bss
    ld.w r11, r8++
    st.w r9++, r11
    rjmp .Lcopy_data

.Lprepare_bss:
    lda.w r9, __bss_start__
    lda.w r10, __bss_end__
    mov r11, 0
.Lzero_bss:
    cp.w r9, r10
    breq .Lcall_main
    st.w r9++, r11
    rjmp .Lzero_bss

.Lcall_main:
    call main
.global __main_returned
__main_returned:
    rjmp __main_returned
.size _start, . - _start
.cpool

/* Instruction entry points, NOT a Cortex-M table of address words.
 * Each architectural exception slot halts at its own address, allowing a
 * debugger to identify the cause. INTC interrupts remain globally masked.
 * Offsets: AT32UC3B datasheet 32059L, table 6-4 (including unused slots).
 */
.section .exception_vectors, "ax", @progbits
.balign 512
.global __exception_vectors
.type __exception_vectors, @function
__exception_vectors:
.macro trap_slot offset
    .org \offset
    rjmp .
.endm
    trap_slot 0x00 /* Unrecoverable exception */
    trap_slot 0x04 /* TLB multiple hit */
    trap_slot 0x08 /* Data bus error */
    trap_slot 0x0c /* Instruction bus error */
    trap_slot 0x10 /* NMI */
    trap_slot 0x14 /* Instruction address */
    trap_slot 0x18 /* ITLB protection */
    trap_slot 0x1c /* Breakpoint */
    trap_slot 0x20 /* Illegal opcode */
    trap_slot 0x24 /* Unimplemented instruction */
    trap_slot 0x28 /* Privilege violation */
    trap_slot 0x2c /* Floating point: unused */
    trap_slot 0x30 /* Coprocessor absent */
    trap_slot 0x34 /* Data address, read */
    trap_slot 0x38 /* Data address, write */
    trap_slot 0x3c /* DTLB protection, read */
    trap_slot 0x40 /* DTLB protection, write */
    trap_slot 0x44 /* DTLB modified: unused */
    trap_slot 0x50 /* ITLB miss */
    trap_slot 0x60 /* DTLB miss, read */
    trap_slot 0x70 /* DTLB miss, write */
    trap_slot 0x100 /* Supervisor call */
.size __exception_vectors, . - __exception_vectors

/* Give the linker a NOBITS input so old binutils marks the stack allocated. */
.section .stack, "aw", @nobits
.balign 8
.skip 8
