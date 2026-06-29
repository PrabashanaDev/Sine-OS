# Sine OS

Sine OS is a lightweight hobby operating system built to explore operating system development, system architecture, and low-level programming. 

The project is currently being developed as a 32-bit x86 system to establish a solid foundation, with an active roadmap to upgrade to a 64-bit (Long Mode) architecture.

## Current Features
- **Bootloader Entry:** Multiboot-compliant entry point (`boot.asm`)
- **Memory Management:** Global Descriptor Table (GDT) initialization
- **Interrupts:** Interrupt Descriptor Table (IDT) setup
- **Display Driver:** Custom VGA text-mode driver with screen scrolling (Green on Black)
- **Keyboard Input:** PS/2 Scancode translation to US-QWERTY

## Roadmap
- [ ] Basic memory allocation
- [ ] Transition to 64-bit Long Mode:
  - [ ] Implement Paging (PAE)
  - [ ] Update GDT for 64-bit descriptors
  - [ ] Recompile kernel for x86_64

## Architecture & Build
- **Core Kernel:** C (`kernel.c`)
- **Hardware Initialization:** NASM Assembly (`boot.asm`, `gdt.asm`, `idt.asm`)

## Getting Started
*(Instructions for compiling the OS using GCC and running it via QEMU will be added in future updates.)*
