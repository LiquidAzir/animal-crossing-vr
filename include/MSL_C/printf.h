#ifndef MSL_PRINTF_H
#define MSL_PRINTF_H

#ifdef TARGET_PC
/* On PC, system headers provide printf family */
#include <stdio.h>
#include <stdarg.h>
#else

#include "stddef.h"
#include "libc/stdarg.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int vprintf(const char*, va_list);
extern int vsprintf(char*, const char*, va_list);
extern int vsnprintf(char*, size_t, const char*, va_list);
extern int printf(const char*, ...);
int snprintf(char* s, size_t n, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif /* !TARGET_PC */

#endif
