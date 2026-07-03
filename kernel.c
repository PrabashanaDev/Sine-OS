#include "string.h"
#include "multiboot.h"
#include "pmm.h"
#include "graphics.h"
#include "mouse.h"

// --- GDT STRUCTURES ---
struct gdt_entry_struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct gdt_entry_struct gdt_entries[3];
struct gdt_ptr_struct   gdt_ptr;

extern void gdt_flush(uint32_t); 

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;
    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

void init_gdt() {
    gdt_ptr.limit = (sizeof(struct gdt_entry_struct) * 3) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    gdt_set_gate(0, 0, 0, 0, 0);                
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); 
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); 

    gdt_flush((uint32_t)&gdt_ptr);
}

// --- IDT STRUCTURES ---
struct idt_entry_struct {
    uint16_t base_low;
    uint16_t sel;
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

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low = (base & 0xFFFF);
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags;
}

// --- HARDWARE I/O PORTS ---
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// --- PIC REMAPPING ---
void pic_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xF9); // Unmask IRQ 1 (Keyboard) and IRQ 2 (Cascade)
    outb(0xA1, 0xEF); // Unmask IRQ 12 (Mouse)
}

// =============================================================
// ==================== DESKTOP GUI DRAWING ====================
// =============================================================

// Color Palette (0x00RRGGBB)
#define COLOR_DESKTOP_TOP  0x001A3A5A
#define COLOR_TASKBAR      0x001E1E2E
#define COLOR_TASKBAR_TOP  0x00313145
#define COLOR_WINDOW_BG    0x001A1A2A
#define COLOR_WINDOW_TITLE 0x002A2A40
#define COLOR_WINDOW_BORDER 0x00404058
#define COLOR_ACCENT       0x006C9BD2
#define COLOR_TEXT_GREEN    0x0040E870
#define COLOR_TEXT_LIGHT   0x00D0D0E0
#define COLOR_START_BTN    0x004A7FB5
#define COLOR_CURSOR       0x0040E870

// Draw a horizontal line
void draw_hline(uint32_t x, uint32_t y, uint32_t width, uint32_t color) {
    for (uint32_t i = 0; i < width; i++) {
        draw_pixel(x + i, y, color);
    }
}

// Draw a vertical line
void draw_vline(uint32_t x, uint32_t y, uint32_t height, uint32_t color) {
    for (uint32_t i = 0; i < height; i++) {
        draw_pixel(x, y + i, color);
    }
}

// Draw a rectangle outline
void draw_rect_outline(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    draw_hline(x, y, w, color);
    draw_hline(x, y + h - 1, w, color);
    draw_vline(x, y, h, color);
    draw_vline(x + w - 1, y, h, color);
}

// =============================================================
// ================ GRAPHICAL TERMINAL ENGINE ==================
// =============================================================

// Terminal window position and size (inside the main window)
#define TERM_WIN_X      30
#define TERM_WIN_Y      30
#define TERM_WIN_W      740
#define TERM_WIN_H      500
#define TERM_TITLE_H    28

// Terminal text area (inside the window body)
#define TERM_PAD        8
#define TERM_TEXT_X     (TERM_WIN_X + TERM_PAD)
#define TERM_TEXT_Y     (TERM_WIN_Y + TERM_TITLE_H + 1 + TERM_PAD)
#define TERM_TEXT_W     (TERM_WIN_W - TERM_PAD * 2)
#define TERM_TEXT_H     (TERM_WIN_H - TERM_TITLE_H - 1 - TERM_PAD * 2)

// Character grid dimensions
#define CHAR_W          8
#define CHAR_H          10   // 8px glyph + 2px line spacing
#define TERM_COLS       (TERM_TEXT_W / CHAR_W)
#define TERM_ROWS       (TERM_TEXT_H / CHAR_H)

// Terminal state
static size_t term_col = 0;
static size_t term_row = 0;

// Forward declarations
void gfx_putchar(char c);
void gfx_print(const char* str);
void term_scroll(void);
void term_clear_row(size_t row);
void draw_cursor(void);
void erase_cursor(void);

// Draw the desktop background and the terminal window frame
void draw_desktop(void) {
    uint32_t sw = get_screen_width();
    uint32_t sh = get_screen_height();
    uint32_t taskbar_height = 36;
    
    // Desktop gradient background
    for (uint32_t y = 0; y < sh - taskbar_height; y++) {
        uint32_t r = 0x1A + (y * 0x20) / (sh - taskbar_height);
        uint32_t g = 0x3A + (y * 0x30) / (sh - taskbar_height);
        uint32_t b = 0x5A + (y * 0x35) / (sh - taskbar_height);
        if (r > 0xFF) r = 0xFF;
        if (g > 0xFF) g = 0xFF;
        if (b > 0xFF) b = 0xFF;
        uint32_t color = (r << 16) | (g << 8) | b;
        draw_hline(0, y, sw, color);
    }
    
    // Taskbar
    draw_rect(0, sh - taskbar_height, sw, taskbar_height, COLOR_TASKBAR);
    draw_hline(0, sh - taskbar_height, sw, COLOR_TASKBAR_TOP);
    
    // Start Button
    draw_rect(4, sh - taskbar_height + 6, 70, 24, COLOR_START_BTN);
    draw_hline(4, sh - taskbar_height + 6, 70, COLOR_ACCENT);
    draw_string(14, sh - taskbar_height + 14, "SineOS", COLOR_TEXT_LIGHT);
    
    // Terminal window border
    draw_rect_outline(TERM_WIN_X - 1, TERM_WIN_Y - 1, TERM_WIN_W + 2, TERM_WIN_H + 2, COLOR_WINDOW_BORDER);
    
    // Terminal window title bar
    draw_rect(TERM_WIN_X, TERM_WIN_Y, TERM_WIN_W, TERM_TITLE_H, COLOR_WINDOW_TITLE);
    draw_string(TERM_WIN_X + 10, TERM_WIN_Y + 10, "Terminal", COLOR_TEXT_LIGHT);
    
    // Close button
    draw_rect(TERM_WIN_X + TERM_WIN_W - 22, TERM_WIN_Y + 6, 16, 16, 0x00E04848);
    // Minimize button
    draw_rect(TERM_WIN_X + TERM_WIN_W - 44, TERM_WIN_Y + 6, 16, 16, 0x00D4A843);
    
    // Accent line under title
    draw_hline(TERM_WIN_X, TERM_WIN_Y + TERM_TITLE_H, TERM_WIN_W, COLOR_ACCENT);
    
    // Terminal body (dark background)
    draw_rect(TERM_WIN_X, TERM_WIN_Y + TERM_TITLE_H + 1, TERM_WIN_W, TERM_WIN_H - TERM_TITLE_H - 1, COLOR_WINDOW_BG);
}

// Clear a single row of the terminal text area
void term_clear_row(size_t row) {
    uint32_t px_y = TERM_TEXT_Y + row * CHAR_H;
    draw_rect(TERM_TEXT_X, px_y, TERM_TEXT_W, CHAR_H, COLOR_WINDOW_BG);
}

// Scroll the terminal up by one row by redrawing
// We shift pixel data up by CHAR_H pixels within the terminal body
void term_scroll(void) {
    uint8_t* fb = get_framebuffer();
    uint32_t pitch = get_screen_pitch();
    uint32_t bpp = get_screen_bpp() / 8;
    
    // Number of pixel rows to shift
    uint32_t total_text_pixel_rows = TERM_ROWS * CHAR_H;
    uint32_t shift_rows = total_text_pixel_rows - CHAR_H;
    
    // Copy each row of pixels up by CHAR_H pixels
    for (uint32_t py = 0; py < shift_rows; py++) {
        uint32_t src_y = TERM_TEXT_Y + py + CHAR_H;
        uint32_t dst_y = TERM_TEXT_Y + py;
        uint32_t src_offset = src_y * pitch + TERM_TEXT_X * bpp;
        uint32_t dst_offset = dst_y * pitch + TERM_TEXT_X * bpp;
        
        // Copy one row of terminal text pixels
        uint8_t* src = fb + src_offset;
        uint8_t* dst = fb + dst_offset;
        for (uint32_t b = 0; b < TERM_TEXT_W * bpp; b++) {
            dst[b] = src[b];
        }
    }
    
    // Clear the bottom row
    term_clear_row(TERM_ROWS - 1);
}

// Draw a blinking cursor block at the current position
void draw_cursor(void) {
    uint32_t px_x = TERM_TEXT_X + term_col * CHAR_W;
    uint32_t px_y = TERM_TEXT_Y + term_row * CHAR_H;
    draw_rect(px_x, px_y, CHAR_W, CHAR_H - 2, COLOR_CURSOR);
}

// Erase the cursor by overwriting with background color
void erase_cursor(void) {
    uint32_t px_x = TERM_TEXT_X + term_col * CHAR_W;
    uint32_t px_y = TERM_TEXT_Y + term_row * CHAR_H;
    draw_rect(px_x, px_y, CHAR_W, CHAR_H - 2, COLOR_WINDOW_BG);
}

// Write a single character to the graphical terminal
void gfx_putchar(char c) {
    erase_cursor();
    
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else if (c == '\b') {
        if (term_col > 0) {
            term_col--;
        } else if (term_row > 0) {
            term_row--;
            term_col = TERM_COLS - 1;
        }
        // Erase the character at the new position
        uint32_t px_x = TERM_TEXT_X + term_col * CHAR_W;
        uint32_t px_y = TERM_TEXT_Y + term_row * CHAR_H;
        draw_rect(px_x, px_y, CHAR_W, CHAR_H, COLOR_WINDOW_BG);
        draw_cursor();
        return;
    } else {
        // Draw the character at the current cursor position
        uint32_t px_x = TERM_TEXT_X + term_col * CHAR_W;
        uint32_t px_y = TERM_TEXT_Y + term_row * CHAR_H;
        draw_char(px_x, px_y, c, COLOR_TEXT_GREEN);
        
        term_col++;
        if (term_col >= TERM_COLS) {
            term_col = 0;
            term_row++;
        }
    }
    
    // Handle scrolling
    if (term_row >= TERM_ROWS) {
        term_scroll();
        term_row = TERM_ROWS - 1;
    }
    
    draw_cursor();
}

// Print a full string to the graphical terminal
void gfx_print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        gfx_putchar(str[i]);
    }
}

// Clear the entire terminal text area
void term_clear(void) {
    draw_rect(TERM_WIN_X, TERM_WIN_Y + TERM_TITLE_H + 1, TERM_WIN_W, TERM_WIN_H - TERM_TITLE_H - 1, COLOR_WINDOW_BG);
    term_col = 0;
    term_row = 0;
    draw_cursor();
}

// =============================================================
// ====================== COMMAND SHELL ========================
// =============================================================

char command_buffer[256];
size_t command_len = 0;

void execute_command(char* input) {
    if (strcmp(input, "help") == 0) {
        gfx_print("Available commands:\n");
        gfx_print("  help  - Show this message\n");
        gfx_print("  clear - Clear the screen\n");
        gfx_print("  echo  - Print text to the screen\n");
        gfx_print("  alloc - Allocate a 4KB memory block\n");
    } else if (strcmp(input, "clear") == 0) {
        term_clear();
    } else if (input[0] == 'e' && input[1] == 'c' && input[2] == 'h' && input[3] == 'o' && input[4] == ' ') {
        gfx_print(&input[5]);
        gfx_print("\n");
    } else if (strcmp(input, "alloc") == 0) {
        uint32_t addr = pmm_alloc_block();
        if (addr == 0) {
            gfx_print("ERROR: Out of Memory!\n");
        } else {
            char hex_buf[16];
            itoa(addr, hex_buf, 16);
            gfx_print("Allocated 4KB block at 0x");
            gfx_print(hex_buf);
            gfx_print("\n");
        }
    } else if (strlen(input) > 0) {
        gfx_print("Unknown command: ");
        gfx_print(input);
        gfx_print("\n");
    }
}

// --- KEYBOARD MAP & HANDLER ---
const char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t',
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',
    0,
  ' ',
    0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
  '-',
    0, 0, 0,
  '+',
    0, 0, 0, 0, 0,
    0, 0, 0,
    0, 0,
    0,
};

void keyboard_handler_c() {
    uint8_t scancode = inb(0x60); 
    
    if (scancode < 128) { 
        char c = keyboard_map[scancode];
        if (c != 0) { 
            if (c == '\b') {
                if (command_len > 0) {
                    command_len--;
                    command_buffer[command_len] = '\0';
                    gfx_putchar(c);
                }
            } 
            else if (c == '\n') {
                gfx_putchar('\n');
                command_buffer[command_len] = '\0';
                execute_command(command_buffer);
                
                command_len = 0;
                command_buffer[0] = '\0';
                gfx_print("SineOS> ");
            } 
            else {
                if (command_len < 255) {
                    command_buffer[command_len] = c;
                    command_len++;
                    gfx_putchar(c);
                }
            }
        }
    }
    
    outb(0x20, 0x20);
}

// Function to load the IDT into the CPU
extern void idt_flush(uint32_t); 
extern void keyboard_handler_isr();
extern void mouse_handler_isr();
void init_idt() {
    idt_ptr.limit = (sizeof(struct idt_entry_struct) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;
    
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    idt_set_gate(33, (uint32_t)keyboard_handler_isr, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)mouse_handler_isr, 0x08, 0x8E);
    idt_flush((uint32_t)&idt_ptr);
}

// --- The OS Entry Point ---
void kernel_main(uint32_t magic, multiboot_info_t* mbd) {
    init_gdt();
    pic_remap();
    init_idt();
    
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        while(1) { asm volatile("hlt"); }
    }
    
    // Initialize PMM
    size_t total_memory_bytes = (mbd->mem_upper * 1024) + (1024 * 1024);
    pmm_init(total_memory_bytes, 0); 
    
    // Initialize graphics
    init_graphics(mbd);
    
    // Initialize Mouse
    mouse_init();
    
    // Enable interrupts
    asm volatile("sti"); 
    
    // Draw the desktop and terminal window
    draw_desktop();
    
    // Print boot messages in the graphical terminal
    gfx_print("Sine OS v0.3 - Graphical Terminal\n");
    gfx_print("GDT Loaded. IDT Loaded. PMM Initialized.\n");
    
    char mem_buf[16];
    itoa(total_memory_bytes / (1024 * 1024), mem_buf, 10);
    gfx_print("Tracking ");
    gfx_print(mem_buf);
    gfx_print(" MB of RAM.\n\n");
    
    gfx_print("Welcome to Sine OS!\n");
    gfx_print("Type 'help' to see available commands.\n\n");
    gfx_print("SineOS> ");
    
    while (1) {
        asm volatile("hlt");
    }
}
