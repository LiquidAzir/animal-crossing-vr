#ifndef _ERRNO_H_
#define _ERRNO_H_

#ifdef TARGET_PC
#include_next <errno.h>
#else

#define ERANGE 32

extern int errno;

#endif /* TARGET_PC */
#endif
