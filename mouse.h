#ifndef MOUSE_H
#define MOUSE_H

#include "string.h"

// Get the current mouse coordinates
int get_mouse_x(void);
int get_mouse_y(void);

// Initialize the PS/2 mouse hardware
void mouse_init(void);

// The C handler called by the assembly ISR when the mouse moves
void mouse_handler_c(void);

#endif
