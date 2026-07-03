#include "mouse.h"
#include "graphics.h"

// I/O Ports
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Global mouse coordinates
static int mouse_x = 400; 
static int mouse_y = 300;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) { // Wait to read
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else { // Wait to write
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

void mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(0x64, 0xD4); // Tell keyboard controller to send to mouse
    mouse_wait(1);
    outb(0x60, write); // Send the data
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init(void) {
    uint8_t status;
    
    // Enable the auxiliary mouse device
    mouse_wait(1);
    outb(0x64, 0xA8);
    
    // Enable the interrupts
    mouse_wait(1);
    outb(0x64, 0x20); // Read compaq status byte
    mouse_wait(0);
    status = (inb(0x60) | 2); // Set bit 1 (enable IRQ 12)
    mouse_wait(1);
    outb(0x64, 0x60); // Write compaq status byte
    mouse_wait(1);
    outb(0x60, status);
    
    // Set mouse to use default settings
    mouse_write(0xF6);
    mouse_read(); // Acknowledge
    
    // Enable mouse data reporting
    mouse_write(0xF4);
    mouse_read(); // Acknowledge
    
    // Draw the initial mouse cursor in the center of the screen
    draw_mouse_cursor(mouse_x, mouse_y);
}

// This is called thousands of times by the assembly ISR when you move the mouse!
void mouse_handler_c(void) {
    uint8_t status = inb(0x64);
    
    // Always read the data byte to clear the buffer, even if we discard it
    uint8_t data = inb(0x60);
    
    // Check if this interrupt was actually from the mouse (bit 5 of status)
    if (!(status & 0x20)) goto send_eoi;
    
    mouse_packet[mouse_cycle++] = data;
    
    if (mouse_cycle == 3) { // We have a full 3-byte packet
        mouse_cycle = 0; // Reset for next packet
        
        // Skip packet if the alignment is wrong (bit 3 should be 1)
        if ((mouse_packet[0] & 0x08) == 0) goto send_eoi;
        
        // Calculate X and Y movement delta
        int d_x = mouse_packet[1];
        int d_y = mouse_packet[2];
        
        // Apply sign extension if negative
        if (mouse_packet[0] & 0x10) d_x |= 0xFFFFFF00;
        if (mouse_packet[0] & 0x20) d_y |= 0xFFFFFF00;
        
        // The mouse hardware Y axis is inverted compared to the screen
        d_y = -d_y;
        
        // Erase old cursor (restore the desktop background)
        erase_mouse_cursor(mouse_x, mouse_y);
        
        // Update coordinates
        mouse_x += d_x;
        mouse_y += d_y;
        
        // Clamp coordinates to screen bounds so the mouse doesn't run off the edge!
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= (int)get_screen_width() - 5) mouse_x = get_screen_width() - 5;
        if (mouse_y >= (int)get_screen_height() - 5) mouse_y = get_screen_height() - 5;
        
        // Draw new cursor at the new position
        draw_mouse_cursor(mouse_x, mouse_y);
    }
    
send_eoi:
    // ALWAYS acknowledge the interrupt to BOTH PICs, even if we discarded the data!
    // Without this, the PIC hangs and ALL interrupts (keyboard too!) stop working.
    outb(0xA0, 0x20); 
    outb(0x20, 0x20);
}

int get_mouse_x(void) { return mouse_x; }
int get_mouse_y(void) { return mouse_y; }

