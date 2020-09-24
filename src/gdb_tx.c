#include <stdint.h>
#include "py/mpconfig.h"
#include "py/obj.h"

#include "general.h"
#include "mod_bmp.h"
#include "gdb_rxtx.h"
#include "gdb_main.h"
#include "gdb_packet.h"
#include "morse.h"
#include "exception.h"
#include "hex_utils.h"
#include "ctype.h"

static const uint8_t hexdigits[] = "0123456789abcdef";

/* send a packet to gdb */
void gdb_putpacket(const char *packet, int size) {
  unsigned char csum = 0; /* running checksum */
  uint8_t csum1, csum2;   /* checksum characters */
  gdb_tx_char('$', false);
  for (int i = 0; i < size; i++) {
    uint8_t ch = packet[i];
    if ((ch == '$') || (ch == '#') || (ch == '}')) {
      gdb_tx_char('}', false);
      csum += '}';
      uint8_t esc_ch = ch ^ 0x20; /* escaped character */
      gdb_tx_char(esc_ch, false);
      csum += esc_ch;
    } else {
      gdb_tx_char(ch, false);
      csum += ch;
    }
  }
  /* send checksum */
  gdb_tx_char('#', false);
  csum1 = hexdigits[(csum >> 4) & 0xF];
  gdb_tx_char(csum1, false);
  csum2 = hexdigits[csum & 0xF];
  gdb_tx_char(csum2, true); // transmit packet
  gdb_rx_ack();        // wait for '+' ack
}

/* send a single character to gdb */
void gdb_putchar(const char ch) {
  gdb_tx_char(ch, true); // transmit packet
}
