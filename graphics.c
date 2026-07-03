#include "graphics.h"
#include "font.h"

// Framebuffer state
static uint8_t*  framebuffer = 0;
static uint32_t  screen_width = 0;
static uint32_t  screen_height = 0;
static uint32_t  screen_pitch = 0;
static uint8_t   screen_bpp = 0;

void init_graphics(multiboot_info_t* mbd) {
    framebuffer   = (uint8_t*)(uint32_t) mbd->framebuffer_addr_low;
    screen_width  = mbd->framebuffer_width;
    screen_height = mbd->framebuffer_height;
    screen_pitch  = mbd->framebuffer_pitch;
    screen_bpp    = mbd->framebuffer_bpp;
}

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= screen_width || y >= screen_height) return;
    
    uint32_t bytes_per_pixel = screen_bpp / 8;
    uint32_t offset = y * screen_pitch + x * bytes_per_pixel;
    
    framebuffer[offset]     = color & 0xFF;           // Blue
    framebuffer[offset + 1] = (color >> 8) & 0xFF;    // Green
    framebuffer[offset + 2] = (color >> 16) & 0xFF;   // Red
}

void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t row = y; row < y + height; row++) {
        for (uint32_t col = x; col < x + width; col++) {
            draw_pixel(col, row, color);
        }
    }
}

void fill_screen(uint32_t color) {
    for (uint32_t y = 0; y < screen_height; y++) {
        for (uint32_t x = 0; x < screen_width; x++) {
            draw_pixel(x, y, color);
        }
    }
}

void draw_char(uint32_t x, uint32_t y, char c, uint32_t color) {
    // Only render printable ASCII characters (32-126)
    if (c < 32 || c > 126) return;
    
    // Look up the 8-byte bitmap for this character
    const unsigned char* glyph = font8x8[c - 32];
    
    // Draw each of the 8 rows
    for (int row = 0; row < 8; row++) {
        unsigned char row_data = glyph[row];
        // Check each of the 8 bits (columns) in this row
        for (int col = 0; col < 8; col++) {
            // MSB = leftmost pixel, so we check bit 7 down to bit 0
            if (row_data & (0x80 >> col)) {
                draw_pixel(x + col, y + row, color);
            }
        }
    }
}

void draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color) {
    uint32_t start_x = x;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            x = start_x;
            y += 10; // 8px char height + 2px line spacing
        } else {
            draw_char(x, y, str[i], color);
            x += 8; // Each character is 8 pixels wide
        }
    }
}

// =============================================================
// ==================== MOUSE CURSOR CACHE =====================
// =============================================================

static uint32_t bg_buffer[5][5];
static int cursor_drawn = 0;
static int last_mouse_x = 0;
static int last_mouse_y = 0;

static uint32_t get_pixel(uint32_t x, uint32_t y) {
    if (x >= screen_width || y >= screen_height) return 0;
    uint32_t offset = y * screen_pitch + x * (screen_bpp / 8);
    uint32_t b = framebuffer[offset];
    uint32_t g = framebuffer[offset + 1];
    uint32_t r = framebuffer[offset + 2];
    return (r << 16) | (g << 8) | b;
}

void erase_mouse_cursor(int x, int y) {
    (void)x; (void)y;
    if (!cursor_drawn) return;
    
    // Restore the exact pixels that were under the mouse before we drew it!
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            draw_pixel(last_mouse_x + col, last_mouse_y + row, bg_buffer[row][col]);
        }
    }
    cursor_drawn = 0;
}

void draw_mouse_cursor(int x, int y) {
    // 1. Take a screenshot of the 5x5 background underneath the new cursor position
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            bg_buffer[row][col] = get_pixel(x + col, y + row);
        }
    }
    
    // 2. Draw a 5x5 White Crosshair Cursor
    uint32_t c = 0x00FFFFFF;
    draw_pixel(x + 2, y + 0, c);
    draw_pixel(x + 2, y + 1, c);
    draw_pixel(x + 0, y + 2, c);
    draw_pixel(x + 1, y + 2, c);
    draw_pixel(x + 2, y + 2, c);
    draw_pixel(x + 3, y + 2, c);
    draw_pixel(x + 4, y + 2, c);
    draw_pixel(x + 2, y + 3, c);
    draw_pixel(x + 2, y + 4, c);
    
    last_mouse_x = x;
    last_mouse_y = y;
    cursor_drawn = 1;
}

uint32_t get_screen_width(void) {
    return screen_width;
}

uint32_t get_screen_height(void) {
    return screen_height;
}

uint8_t* get_framebuffer(void) {
    return framebuffer;
}

uint32_t get_screen_pitch(void) {
    return screen_pitch;
}

uint8_t get_screen_bpp(void) {
    return screen_bpp;
}
