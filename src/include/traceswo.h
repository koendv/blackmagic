#ifndef __TRACESWO_H
#define __TRACESWO_H

#include "py/obj.h"
#include "py/stream.h"

extern bool swo_enabled;                // true if decoding swo
extern uint32_t swo_channelmask;        // bitmask of channels to print
extern mp_obj_t swo_uart;               // swo connected to rx pin of this uart
extern const mp_stream_p_t *swo_stream; // stream corresponding to uart

void traceswo_decode();                 // print decoded swo channels

#endif
