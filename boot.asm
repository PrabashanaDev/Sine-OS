MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
VIDMODE  equ  1 << 2            ; Request framebuffer video mode from GRUB
FLAGS    equ  MBALIGN | MEMINFO | VIDMODE
MAGIC    equ  0x1BADB002       ; 'Magic number' lets the bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0           ; Address fields (unused, set to 0)
    dd 0                        ; mode_type: 0 = linear graphics mode
    dd 800                      ; width:  800 pixels
    dd 600                      ; height: 600 pixels
    dd 32                       ; depth:  32 bits per pixel (True Color)

section .bss
align 16
stack_bottom:
    resb 16384                 ; Reserve 16KB for the stack
stack_top:

section .text
global _start
extern kernel_main             ; Points to our C function

_start:
    mov esp, stack_top         ; Set up the stack pointer
    push ebx                   ; Push pointer to the Multiboot information structure
    push eax                   ; Push Multiboot magic number
    call kernel_main           ; Jump into the C code!
    
    cli                        ; Disable interrupts
.hang:
    hlt                        ; Halt the CPU
    jmp .hang                  ; Loop indefinitely if CPU wakes up
