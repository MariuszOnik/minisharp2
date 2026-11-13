/* MiniSharp - built-in library (layer 2): classes that do not depend on the platform.
 * The host calls lib_register_all() once before the first ms_compile. */
#ifndef MS_LIB_H
#define MS_LIB_H

/* Where Console.Write/WriteLine go. The host sets it (3DS: the terminal on screen); NULL = stdout. */
void lib_set_output(void (*write)(const char* s));

/* One function per class group; lib_register_all calls them in the old natives.c order. */
void lib_register_console(void);    /* Console */
void lib_register_math(void);       /* Math, Mathf (+ PI, E, Deg2Rad…) */
void lib_register_random(void);     /* Random */
void lib_register_strings(void);    /* string methods, string.IsNullOrEmpty */
void lib_register_convert(void);    /* int/float.Parse, int.MaxValue/MinValue, char.IsDigit… */
void lib_register_all(void);
/* List<T> and Dictionary<K,V> are part of the language (lang/vm.c), not of these tables. */

#endif
