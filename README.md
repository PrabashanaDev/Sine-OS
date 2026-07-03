# Sine OS

Sine OS is a lightweight hobby operating system built to explore operating system development, system architecture, and low-level programming. Designed to run smoothly on both modern and legacy hardware.

The project is currently being developed as a 32-bit x86 system to establish a solid foundation, with an active roadmap to upgrade to a 64-bit (Long Mode) architecture.

## Current Features
- **Bootloader Entry:** Multiboot-compliant entry point (`boot.asm`)
- **Memory Management:** Global Descriptor Table (GDT) and Physical Memory Manager (PMM) via Bitmap Allocation
- **Interrupts:** Interrupt Descriptor Table (IDT) with PIC remapping
- **Graphics Engine:** VESA Framebuffer (800x600, 32-bit True Color) with gradient desktop, taskbar, and window rendering
- **Bitmap Font Renderer:** 8x8 pixel font engine for drawing text in graphical mode
- **Graphical Terminal:** Interactive command shell rendered inside a GUI window with scrolling
- **Keyboard Input:** PS/2 Scancode translation to US-QWERTY
- **Standard Library:** Basic libc functions (`strlen`, `strcmp`, `memset`, `itoa`)
- **Bootable ISO:** GRUB-based ISO image for emulators and real hardware

## Roadmap
- [ ] Mouse driver support
- [ ] Transition to 64-bit Long Mode:
  - [ ] Implement Paging (PAE)
  - [ ] Update GDT for 64-bit descriptors
  - [ ] Recompile kernel for x86_64

## Architecture & Build
- **Core Kernel:** C (`kernel.c`)
- **Graphics Engine:** C (`graphics.c`, `graphics.h`)
- **Memory Manager:** C (`pmm.c`, `pmm.h`)
- **Standard Library:** C (`string.c`, `string.h`)
- **Hardware Initialization:** NASM Assembly (`boot.asm`, `gdt.asm`, `idt.asm`)

## Getting Started
```bash
# Compile
nasm -f elf32 boot.asm -o boot.o
gcc -m32 -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2
gcc -m32 -c graphics.c -o graphics.o -std=gnu99 -ffreestanding -O2
gcc -m32 -c pmm.c -o pmm.o -std=gnu99 -ffreestanding -O2
gcc -m32 -c string.c -o string.o -std=gnu99 -ffreestanding -O2
ld -m elf_i386 -T linker.ld -o myos.bin boot.o gdt.o idt.o string.o pmm.o graphics.o kernel.o

# Create bootable ISO
mkdir -p isodir/boot/grub
cp myos.bin isodir/boot/myos.bin
grub-mkrescue -o sineos.iso isodir

# Run in QEMU
qemu-system-i386 -cdrom sineos.iso -m 32
```
