#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "string.h"
#include "multiboot.h"

// Initialize the graphics engine from Multiboot framebuffer info
void init_graphics(multiboot_info_t* mbd);

// Draw a single pixel at (x, y) with an RGB color
void draw_pixel(uint32_t x, uint32_t y, uint32_t color);

// Draw a filled rectangle
void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);

// Fill the entire screen with a single color
void fill_screen(uint32_t color);

// Draw a single 8x8 character at pixel position (x, y)
void draw_char(uint32_t x, uint32_t y, char c, uint32_t color);

// Draw a null-terminated string starting at pixel position (x, y)
void draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color);

// Get the screen width
uint32_t get_screen_width(void);

// Get the screen height
uint32_t get_screen_height(void);

// Get the framebuffer pointer (needed for scrolling)
uint8_t* get_framebuffer(void);

// Get the screen pitch (bytes per row)
uint32_t get_screen_pitch(void);

// Get bits per pixel
uint8_t get_screen_bpp(void);

#endif
