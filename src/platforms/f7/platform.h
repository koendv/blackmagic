#ifndef MICROPYTHON_PLATFORM_H
#define MICROPYTHON_PLATFORM_H

#include "bmp_malloc.h"
#include "timing.h"
#include "mpconfigboard.h"
#include <stdarg.h>

#define DEBUG DEBUG_WARN

/* comment out PLATFORM_HAS_DEBUG for a faster, smaller binary */
#define PLATFORM_HAS_DEBUG

#define PLATFORM_HAS_HARDWARE_CRC

/* clock */
extern uint32_t platform_time_ms();
extern uint32_t unix_time_ms(); /* time in ms since jan 1, 1970 */

/* debugging */
extern bool debug_bmp;
void debug_printf(const char *fmt, ...);
void debug_vprintf(const char *fmt, va_list ap);

#endif
