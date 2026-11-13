/* MiniSharp - app/ui.h on the 3DS (citro2d): two screens, 400x240 on top, 320x240 below with the touch. */
#ifndef MS_UI_3DS_H
#define MS_UI_3DS_H

#include "ui.h"

/* switches the screens from the libctru text console to citro2d and sets the ui.h backend; 0 = failed.
 * ui3ds_exit gives the screens back (the caller then calls consoleInit again). */
int ui3ds_init(void);
void ui3ds_exit(void);

void ui3ds_frame_begin(uint32_t topBg, uint32_t bottomBg);  /* clears both screens, selects the top one */
void ui3ds_frame_end(void);
void ui3ds_input(UiInput* in);                              /* after hidScanInput() */

#endif
