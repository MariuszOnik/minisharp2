/* MiniSharp - built-in library (layer 2): classes that do not depend on the platform.
 * The host calls lib_register_all() once before the first ms_compile. */
#ifndef MS_LIB_H
#define MS_LIB_H

/* Where Console.Write/WriteLine go. The host sets it (3DS: the terminal on screen); NULL = stdout. */
void lib_set_output(void (*write)(const char* s));

void lib_register_console(void);
void lib_register_all(void);    /* every table of lib/ (Math, List, string… come in stage 2) */

#endif
