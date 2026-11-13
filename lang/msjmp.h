/* MiniSharp - <setjmp.h> for the parser and the compiler, which leave on an error with longjmp.
 * On 64-bit Windows, MinGW's setjmp records the frame for an SEH unwind, and longjmp through our frames crashed
 * at random (0xC0000005 in about 3% of the processes after a single compile error). We only need a plain jump
 * (no C++ destructors, no SEH handlers in between), so we ask for the non-SEH variant: _setjmp(buf, NULL).
 * Include this instead of <setjmp.h>, before any other header that may include it. Other platforms: no change. */
#ifndef MS_MSJMP_H
#define MS_MSJMP_H

#if defined(_WIN64) && defined(__MINGW64__) && !defined(__USE_MINGW_SETJMP_NON_SEH)
#define __USE_MINGW_SETJMP_NON_SEH
#endif
#include <setjmp.h>

#endif
