global gdt_flush

gdt_flush:
    mov eax, [esp+4]  ; Get the pointer to the GDT passed from our C code
    lgdt [eax]        ; Load the new GDT into the CPU

    ; Reload data segment registers with our new Data Segment (offset 0x10)
    mov ax, 0x10      
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Do a "far jump" to reload the Code Segment (offset 0x08)
    jmp 0x08:.flush   
.flush:
    ret
