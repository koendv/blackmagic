#ifndef GDB_RX_H
#define GDB_RX_H

#include "py/obj.h"

#define GDB_MAX_PACKET_SIZE 1024
extern void gdb_rx_char(uint8_t ch);
/* sent gdb command, expecting ack '+' from gdb */
extern void gdb_rx_ack();
extern void gdb_tx_char(uint8_t ch, bool flush);
extern void gdb_rxtx();
void gdb_rxtx_init_tcp(mp_int_t gdb_tcp_port);
void gdb_rxtx_init_stream(mp_obj_t mp_strm);
void gdb_rxtx_deinit();

#endif

