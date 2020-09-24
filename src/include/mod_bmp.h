#ifndef BMP_IF_H
#define BMP_IF_H

// Micro-python to black magic probe interface

#include "general.h"

// enables bmp_loop
extern bool bmp_enabled;

// display console message
extern void bmp_display(const uint8_t *msg, size_t msg_len);

// execute python command from gdb
// e.g. 'monitor python 1+1'
extern int bmp_repl(const char * cmd);

#endif
