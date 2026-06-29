MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
FLAGS    equ  MBALIGN | MEMINFO
MAGIC    equ  0x1BADB002       ; 'Magic number' lets the bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

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
    call kernel_main           ; Jump into the C code!
    
    cli                        ; Disable interrupts
.hang:
    hlt                        ; Halt the CPU
    jmp .hang                  ; Loop indefinitely if CPU wakes up
