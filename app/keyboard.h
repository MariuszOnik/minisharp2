/* MiniSharp - touch keyboard drawn by code (terminal, later the editor). Three layers: letters, capitals, symbols
 * for code; 5 rows of 10 units. Only app/ui.h: the layout and the hits are tested on the PC (tests/unit/test_keyboard.c). */
#ifndef MS_KEYBOARD_H
#define MS_KEYBOARD_H

#include "ui.h"

#define KBD_BACKSPACE "\b"
#define KBD_ENTER "\n"

void kbd_reset(void);                           /* letters, Shift off, nothing pressed */
/* where the keyboard is on the touch screen (default: the 3DS bottom screen below y = 80: 0, 80, 320, 160) */
void kbd_set_area(float x, float y, float w, float h);
void kbd_set_enter_label(const char* s);        /* "Run" in the terminal, "Enter" in the editor */

/* once per frame: text to insert (or KBD_BACKSPACE / KBD_ENTER), or NULL. Holding a key repeats it. */
const char* kbd_update(const UiInput* in);
void kbd_draw(void);                            /* on the selected screen */

/* for tests and hints: the centre of the key with this word on the current layer ("a", "SHIFT", "+=" ...); 0 = none */
int kbd_key_center(const char* word, float* x, float* y);
int kbd_layer(void);                            /* 0 letters, 1 capitals, 2 symbols */
int kbd_shift(void);                            /* 0 off, 1 once, 2 locked */

#endif
