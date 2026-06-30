#include "string.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

const size_t VGA_WIDTH = 80;
const size_t VGA_HEIGHT = 25;
uint16_t* terminal_buffer = (uint16_t*) 0xB8000;

// These variables keep track of where the cursor is
size_t terminal_row = 0;
size_t terminal_column = 0;
uint8_t terminal_color = 2; // 2 = Green text on Black background // White on black

// --- GDT STRUCTURES ---
struct gdt_entry_struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)); // 'packed' prevents the compiler from messing with memory alignment

struct gdt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct gdt_entry_struct gdt_entries[3];
struct gdt_ptr_struct   gdt_ptr;

// This links to the assembly function we just wrote in gdt.asm
extern void gdt_flush(uint32_t); 

// Function to populate a single GDT entry
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;
    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

// Function to set up the whole table
void init_gdt() {
    gdt_ptr.limit = (sizeof(struct gdt_entry_struct) * 3) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    // The CPU requires the very first entry to be completely zeroed out (Null Segment)
    gdt_set_gate(0, 0, 0, 0, 0);                
    // Segment 1: Kernel Code Segment (Exec/Read, covers all 4GB)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); 
    // Segment 2: Kernel Data Segment (Read/Write, covers all 4GB)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); 

    // Tell the CPU to apply these changes!
    gdt_flush((uint32_t)&gdt_ptr);
}

// --- IDT STRUCTURES ---
struct idt_entry_struct {
    uint16_t base_low;
    uint16_t sel;        // Kernel segment
    uint8_t  always0;    
    uint8_t  flags;      
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct idt_entry_struct idt_entries[256];
struct idt_ptr_struct   idt_ptr;

// Function to set an entry in the IDT
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low = (base & 0xFFFF);
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags;
}

void print_string(const char* data);
void terminal_putchar(char c);
void terminal_initialize(void);

// --- HARDWARE I/O PORTS ---
// Write data to a hardware port
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}
// Read data from a hardware port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// --- PIC REMAPPING ---
// By default, hardware interrupts clash with CPU errors. 
// We must remap them to start at Interrupt 32.
void pic_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20); // Master PIC offset (Interrupt 32)
    outb(0xA1, 0x28); // Slave PIC offset (Interrupt 40)
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFD); // Mask all except Keyboard (IRQ 1)
    outb(0xA1, 0xFF); // Mask all slave interrupts
}

// --- KEYBOARD MAP & HANDLER ---
// Standard US QWERTY Scancode lookup table
const char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', /* Backspace */
  '\t', /* Tab */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter */
    0, /* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, /* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, /* Right shift */
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0, /* All other keys are undefined */
};

// --- COMMAND SHELL ---
char command_buffer[256];
size_t command_len = 0;

void execute_command(char* input) {
    if (strcmp(input, "help") == 0) {
        print_string("Available commands:\n");
        print_string("  help  - Show this message\n");
        print_string("  clear - Clear the screen\n");
        print_string("  echo  - Print text to the screen\n");
    } else if (strcmp(input, "clear") == 0) {
        terminal_initialize();
    } else if (input[0] == 'e' && input[1] == 'c' && input[2] == 'h' && input[3] == 'o' && input[4] == ' ') {
        print_string(&input[5]);
        print_string("\n");
    } else if (strlen(input) > 0) {
        print_string("Unknown command: ");
        print_string(input);
        print_string("\n");
    }
}

// This is the function the CPU jumps to when you press a key!
void keyboard_handler_c() {
    uint8_t scancode = inb(0x60); 
    
    if (scancode < 128) { 
        char c = keyboard_map[scancode];
        if (c != 0) { 
            // Handle Backspace
            if (c == '\b') {
                if (command_len > 0) {
                    command_len--;
                    command_buffer[command_len] = '\0';
                    terminal_putchar(c); // Erase from screen
                }
            } 
            // Handle Enter
            else if (c == '\n') {
                terminal_putchar('\n');
                command_buffer[command_len] = '\0';
                execute_command(command_buffer);
                
                // Reset buffer and print prompt
                command_len = 0;
                command_buffer[0] = '\0';
                print_string("SineOS> ");
            } 
            // Handle Normal Characters
            else {
                if (command_len < 255) {
                    command_buffer[command_len] = c;
                    command_len++;
                    terminal_putchar(c);
                }
            }
        }
    }
    
    outb(0x20, 0x20);
}

// Function to load the IDT into the CPU
extern void idt_flush(uint32_t); 
extern void keyboard_handler_isr(); // Link to assembly
void init_idt() {
    idt_ptr.limit = (sizeof(struct idt_entry_struct) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;
    
    // Clear the table
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // MAP INTERRUPT 33 TO THE KEYBOARD HANDLER
    // 0x08 is our Kernel Code Segment, 0x8E means "32-bit Interrupt Gate"
    idt_set_gate(33, (uint32_t)keyboard_handler_isr, 0x08, 0x8E);

    idt_flush((uint32_t)&idt_ptr);
}


// 1. Initialize the terminal and clear the screen
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = (uint16_t) ' ' | (uint16_t) terminal_color << 8;
        }
    }
}

// 2. The core function: Write a single character and advance the cursor
void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else if (c == '\b') { // Handle backspace
        if (terminal_column > 0) {
            terminal_column--;
        } else if (terminal_row > 0) {
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
        }
        // Clear the character at the new cursor position
        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        terminal_buffer[index] = (uint16_t) ' ' | (uint16_t) terminal_color << 8;
        return; // Exit early so we don't print a weird symbol
    } else {
        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        terminal_buffer[index] = (uint16_t) c | (uint16_t) terminal_color << 8;
        terminal_column++;
        if (terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
        }
    }

    // SCROLLING LOGIC
    // If the row reaches the bottom of the screen (25), shift everything up!
    if (terminal_row == VGA_HEIGHT) {
        // 1. Copy rows 1-24 up to rows 0-23
        for (size_t y = 1; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_buffer[(y - 1) * VGA_WIDTH + x] = terminal_buffer[y * VGA_WIDTH + x];
            }
        }
        // 2. Clear the very bottom row (row 24)
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t) ' ' | (uint16_t) terminal_color << 8;
        }
        // 3. Keep the cursor on the bottom row
        terminal_row = VGA_HEIGHT - 1;
    }
}

// 3. Write a full string of text
void print_string(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++) {
        terminal_putchar(data[i]);
    }
}

// --- The OS Entry Point ---
void kernel_main(void) {
    // Set up our clean terminal
    terminal_initialize();
    init_gdt();
    
    // Initialize Interrupts
    pic_remap();
    init_idt();
    
    // 'sti' stands for Set Interrupts (turns the listener on)
    asm volatile("sti"); 
    
    print_string("Terminal Engine Initialized.\n");
    print_string("GDT Loaded: Kernel now has memory authority!\n");
    print_string("IDT Loaded: Keyboard interrupts enabled!\n");
    
    print_string("\nWelcome to Sine OS!\n");
    print_string("Type 'help' to see available commands.\n\n");
    print_string("SineOS> ");
    
    // Enter an infinite loop so the kernel never returns
    // 'hlt' puts the CPU to sleep until the next interrupt fires
    while (1) {
        asm volatile("hlt");
    }
}
