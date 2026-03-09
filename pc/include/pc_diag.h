#ifndef PC_DIAG_H
#define PC_DIAG_H

/**
 * PC_DIAG - Rate-limited diagnostic printf for bring-up tracing.
 *
 * Usage:  PC_DIAG(limit, fmt, ...)
 *   limit - max number of times this message will print (uses a static counter)
 *   fmt   - printf format string (automatically prefixed with "[PC] ")
 *
 * Compiled out entirely unless PC_VERBOSE_DIAG is defined.
 * Include inside #ifdef TARGET_PC in decomp files.
 */
#ifdef PC_VERBOSE_DIAG
  #include <stdio.h>
  #define PC_DIAG(limit, fmt, ...) \
    do { static int _pc_d = 0; if (_pc_d < (limit)) { printf("[PC] " fmt, ##__VA_ARGS__); _pc_d++; } } while(0)
#else
  #define PC_DIAG(limit, fmt, ...) ((void)0)
#endif

#endif /* PC_DIAG_H */
