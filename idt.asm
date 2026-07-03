global idt_flush
global keyboard_handler_isr
global mouse_handler_isr
extern keyboard_handler_c
extern mouse_handler_c

; Load the IDT table
idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret

; The Interrupt Service Routine (ISR) wrapper for the keyboard
keyboard_handler_isr:
    pushad              ; Save all CPU registers
    cld                 ; Clear direction flag
    call keyboard_handler_c ; Call our C function!
    popad               ; Restore all CPU registers
    iretd               ; Return from the interrupt

; The Interrupt Service Routine (ISR) wrapper for the mouse
mouse_handler_isr:
    pushad              ; Save all CPU registers
    cld                 ; Clear direction flag
    call mouse_handler_c ; Call our C function!
    popad               ; Restore all CPU registers
    iretd               ; Return from the interrupt
