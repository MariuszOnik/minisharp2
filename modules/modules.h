/* MiniSharp2 - modules built into this application (modules/<name>/). A module is written in C, registers itself
 * with ms_register_module and is turned on by a program with 'using Name;' (lang/ms.h). Rules: modules/README.md. */
#ifndef MS_MODULES_H
#define MS_MODULES_H

/* The host calls it once, after lib_register_all and before the first ms_compile. */
void modules_register_all(void);

#endif
