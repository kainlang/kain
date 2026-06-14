/* ============================================================================
 *  KAINOS x86-64 Linker Script — multiboot2-compatible kernel ELF
 *  Stream: A | File: linker.ld
 *
 *  Memory layout:
 *    - Kernel loaded at 1MB (0x100000) by Multiboot2 bootloader
 *    - .multiboot section placed in first 8KB (Multiboot2 requirement)
 *    - .text at 1MB (read-only, executable)
 *    - .rodata after .text (read-only, non-executable)
 *    - .data after .rodata (read-write)
 *    - .bss after .data (zero-initialized at boot)
 *    - 64KB kernel stack in BSS
 *    - Kernel heap starts after .bss + stack, grows upward
 * ============================================================================ */

ENTRY(_start)

SECTIONS
{
    /* Kernel is loaded at 1MB by GRUB2/Multiboot2 */
    . = 1M;

    /* ── Multiboot2 header MUST be in first 8KB ─────────────────── */
    .multiboot : ALIGN(8)
    {
        KEEP(*(.multiboot))
    }

    /* ── Text (code) ────────────────────────────────────────────── */
    .text : ALIGN(4K)
    {
        *(.text .text.*)
    }

    /* ── Read-only data ─────────────────────────────────────────── */
    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    }

    /* ── Read-write data ────────────────────────────────────────── */
    .data : ALIGN(4K)
    {
        *(.data .data.*)
    }

    /* ── BSS (zero-initialized) ─────────────────────────────────── */
    .bss : ALIGN(4K)
    {
        bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        /* Kernel stack — 64KB, 16-byte aligned */
        . = ALIGN(16);
        kernel_stack_bottom = .;
        . = . + 64K;
        kernel_stack_top = .;
        bss_end = .;
    }

    /* End of kernel image — kernel heap starts here */
    _kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.eh_frame)
    }
}
