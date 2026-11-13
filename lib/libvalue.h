/* MiniSharp - helpers shared by the lib/ files (making values, reading strings); not for hosts. */
#ifndef MS_LIBVALUE_H
#define MS_LIBVALUE_H
#include "ms.h"

static inline Value vint(int32_t i) { Value v; v.t = V_INT; v.as.i = i; return v; }
static inline Value vfloat(float f) { Value v; v.t = V_FLOAT; v.as.f = f; return v; }
static inline Value vbool(int b) { Value v; v.t = V_BOOL; v.as.i = b ? 1 : 0; return v; }
static inline Value vnone(void) { Value v; v.t = V_NULL; v.as.o = NULL; return v; }
static inline Value vchar(int c) { Value v; v.t = V_CHAR; v.as.i = c & 0xFFFF; return v; }
static inline const char* vstr(Value v) { return v.t == V_OBJ ? ((ObjString*)v.as.o)->chars : ""; }

/* short type names for the NativeDef tables */
#define F  T_FLOAT
#define I  T_INT
#define S  T_STRING
#define B  T_BOOL
#define A  T_ANY
#define VD T_VOID
#define CH T_CHAR
#define SA T_STRARR

#endif
