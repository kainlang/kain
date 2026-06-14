; ============================================================================
;  KAINOS x86-64 Boot Stub — Multiboot2 entry point
;  Stream: A | File: boot.asm
;
;  This assembly stub:
;  1. Contains the Multiboot2 header for GRUB2 / QEMU -kernel
;  2. Requests memory map + framebuffer info from bootloader
;  3. Sets up a temporary 64KB kernel stack in BSS
;  4. Zeroes the BSS section
;  5. Jumps to kainos_arch_init (defined in arch/x86_64/boot.kn)
;
;  Built with NASM: nasm -f elf64 boot.asm -o boot.o
; ============================================================================

[BITS 64]
[GLOBAL _start]
[EXTERN kainos_arch_init]
[EXTERN bss_start]
[EXTERN bss_end]
[EXTERN kernel_stack_top]

; ── Multiboot2 Header ──────────────────────────────────────────────
;  Must be 64-bit aligned and within first 32768 bytes of kernel image.
;  Placed in .multiboot section to guarantee this via linker script.
section .multiboot
align 8
multiboot2_header_start:
    dd 0xE85250D6                      ; magic
    dd 0                               ; architecture: i386 (0)
    dd multiboot2_header_end - multiboot2_header_start  ; header length
    dd -(0xE85250D6 + 0 + (multiboot2_header_end - multiboot2_header_start))  ; checksum

    ; ── Tag: Memory map request (type=6) ───────────────────────────
    align 8
    dw 6                               ; type: memory map
    dw 0                               ; flags
    dd 24                              ; size: 8 (tag header) + 16 (request)
    dd 0                               ; entry_size = 0 (use bootloader default)
    dd 0                               ; entry_version = 0 (use bootloader default)

    ; ── Tag: Framebuffer request (type=5) ──────────────────────────
    align 8
    dw 5                               ; type: framebuffer
    dw 0                               ; flags
    dd 28                              ; size: 8 + 20
    dd 1024                            ; width  (requested)
    dd 768                             ; height (requested)
    dd 32                              ; depth  (requested)
    dd 0                               ; reserved

    ; ── Tag: Boot device request (type=?) ── skip for minimal boot ─

    ; ── End tag (type=0, size=8) — mandatory, terminates tag list ──
    align 8
    dw 0                               ; type: end
    dw 0                               ; flags
    dd 8                               ; size

multiboot2_header_end:

; ── Kernel Entry Point ──────────────────────────────────────────────
section .text
_start:
    ; At this point (GRUB2 Multiboot2 convention):
    ;   RAX = Multiboot2 magic (must be 0x36D76289)
    ;   RBX = Pointer to Multiboot2 info structure

    ; Verify Multiboot2 magic
    cmp eax, 0x36D76289
    jne .no_multiboot

    ; ── Set up kernel stack (64KB in BSS, 16-byte aligned) ─────────
    ; Stack grows downward from kernel_stack_top
    mov rsp, kernel_stack_top
    and rsp, ~0xF                      ; enforce 16-byte alignment

    ; ── Zero BSS section ───────────────────────────────────────────
    ; bss_start and bss_end are defined in the linker script
    mov rdi, bss_start
    mov rcx, bss_end
    sub rcx, rdi                        ; rcx = BSS size in bytes
    xor rax, rax                        ; fill with zeros
    shr rcx, 3                          ; divide by 8 for qword count
    rep stosq                           ; zero BSS 8 bytes at a time

    ; ── Preserve Multiboot info across call ────────────────────────
    ; x86-64 System V ABI: first two args in RDI, RSI
    ; rax = magic → rdi (first arg)
    ; rbx = info ptr → rsi (second arg)
    mov rdi, rax                        ; multiboot magic
    mov rsi, rbx                        ; multiboot info structure pointer

    ; ── Call Kain arch init ────────────────────────────────────────
    call kainos_arch_init

    ; kainos_arch_init should never return — if it does, halt forever
.halt:
    cli
    hlt
    jmp .halt

.no_multiboot:
    ; No Multiboot2 magic — halt with deadbeef in EAX for debugging
    mov eax, 0xDEADBEEF
    cli
    hlt
    jmp .halt

; ── BSS Section (uninitialized data) ───────────────────────────────
;  kernel_stack_bottom / kernel_stack_top defined in linker script via .bss
section .bss
; Symbols provided by linker.ld — see above for actual reserve.
