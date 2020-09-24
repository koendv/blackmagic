#include "general.h"
#include "mod_bmp.h"
#include "gdb_rxtx.h"
#include "gdb_main.h"
#include "gdb_packet.h"
#include "traceswo.h"
#include "morse.h"
#include "exception.h"
#include "hex_utils.h"
#include "ctype.h"

static char pbuf[GDB_MAX_PACKET_SIZE + 1]; /* gdb packet buffer */
static int size = 0;                       /* size of gdb packet */
static unsigned char csum = 0;             /* running checksum */
static unsigned char csum1, csum2;         /* checksum characters */

typedef enum {
  RX_IDLE,
  RX_CHAR,
  RX_ESC,
  RX_CSUM1,
  RX_CSUM2,
  RX_ACK
} rx_state_t;

static rx_state_t rx_state = RX_IDLE;

void gdb_rx_char(uint8_t ch) {
  DEBUG_INFO("%s: ch %c (%02X)", __func__, isprint(ch) ? ch : ' ', ch);
#ifdef ENABLE_DEBUG
  static char *state_str[] = {"RX_IDLE",  "RX_CHAR",  "RX_ESC",
                              "RX_CSUM1", "RX_CSUM2", "RX_ACK"};
  DEBUG_INFO(" %s ",
             rx_state < sizeof(state_str) ? state_str[rx_state] : "ERROR");
#endif
  switch (rx_state) {
  case RX_IDLE:
    if (ch == '$') {
      rx_state = RX_CHAR;
      size = 0;
      csum = 0;
    } else if (ch == 0x03) /* ctrl-c */
      gdb_halt_target();
    break;
  case RX_CHAR:
    if (size == sizeof(pbuf)) {
      DEBUG_WARN("pbuf overflow\n");
      gdb_putchar('-'); /* send nack */
      rx_state = RX_IDLE;
      size = 0;
      csum = 0;
    } else if (ch == '$') {
      rx_state = RX_IDLE;
      size = 0;
      csum = 0;
    } else if (ch == '#') {
      rx_state = RX_CSUM1;
    } else if (ch == '}') {
      rx_state = RX_ESC;
      csum += ch;
    } else {
      pbuf[size++] = ch;
      csum += ch;
    }
    break;
  case RX_ESC:
    if (size == sizeof(pbuf)) {
      DEBUG_WARN("pbuf overflow\n");
      gdb_putchar('-'); /* send nack */
      rx_state = RX_IDLE;
      size = 0;
      csum = 0;
    } else {
      rx_state = RX_CHAR;
      pbuf[size++] = ch ^ 0x20;
      csum += ch;
    }
    break;
  case RX_CSUM1:
    csum1 = ch;
    rx_state = RX_CSUM2;
    break;
  case RX_CSUM2:
    csum2 = ch;
    if (isxdigit(csum1) && isxdigit(csum2) &&
        (csum == (unhex_digit(csum1) * 16 + unhex_digit(csum2)))) {
      gdb_putchar('+'); /* checksum ok, send ack */
      pbuf[size] = '\0';
      {
        /* execute gdb command */
        volatile struct exception e;
        TRY_CATCH(e, EXCEPTION_ALL) { gdb_main(pbuf, size); }
        if (e.type) {
          gdb_putpacketz("EFF");
          target_list_free();
          morse("TARGET LOST ", 1);
        }
      }
    } else {
      DEBUG_WARN("checksum fail\n");
      gdb_putchar('-'); /* checksum fail, send nack */
    }
    rx_state = RX_IDLE;
    size = 0;
    csum = 0;
    break;
  case RX_ACK:
    /* gdb command sent, expecting ack '+' from gdb */
    if (ch == '$') {
      rx_state = RX_CHAR;
      size = 0;
      csum = 0;
    } else if (ch == '-') {
      DEBUG_WARN("ack error\n");
      rx_state = RX_IDLE;
      size = 0;
      csum = 0;
    }
    if (ch == '+') {
      rx_state = RX_IDLE;
      size = 0;
      csum = 0;
    }
    break;
  default:
    DEBUG_WARN("%s default\n", __func__);
    rx_state = RX_IDLE;
    size = 0;
    csum = 0;
    break;
  }
#ifdef ENABLE_DEBUG
  DEBUG_INFO("-> %s\n",
             rx_state < sizeof(state_str) ? state_str[rx_state] : "ERROR");
#endif
}

void gdb_rx_ack() { rx_state = RX_ACK; }
