/* SWO decoding */

#include "traceswo.h"
#include "general.h"
#include "py/mphal.h"
#include "mod_bmp.h"
#include "ctype.h"

#ifndef SWO_BUFFER_SIZE
#define SWO_BUFFER_SIZE 512
#endif

bool swo_enabled = false;        // true if decoding
uint32_t swo_channelmask = 0;    // bitmask of channels to print
mp_obj_t swo_uart;               // swo connected to rx pin of this uart
const mp_stream_p_t *swo_stream; // stream corresponding to uart

static int swo_pkt_len = 0;                 // decoder state
static uint8_t swo_tx_buf[SWO_BUFFER_SIZE]; // decoded traceswo output
static int swo_tx_len = 0;     // number of characters in swo_tx_buf
static bool swo_print = false; // print current channel?
static uint32_t swo_timestamp = UINT32_MAX; // timestamp of last xmit

/* print decoded swo packets */

void traceswo_decode() {
  mp_int_t errcode = 0;
  mp_uint_t flags;
  mp_uint_t swo_rx_len;
  uint8_t swo_rx_buf[SWO_BUFFER_SIZE];

  if (swo_stream == NULL)
    return;

  flags =
      swo_stream->ioctl(swo_uart, MP_STREAM_POLL, MP_STREAM_POLL_RD, &errcode);
  if (flags & MP_STREAM_POLL_RD)
    swo_rx_len =
        swo_stream->read(swo_uart, swo_rx_buf, SWO_BUFFER_SIZE, &errcode);
  else
    swo_rx_len = 0;

  if ((swo_rx_len == 0) && (swo_tx_len != 0)) {
    // don't leave chars stuck in buffer forever
    uint32_t now = mp_hal_ticks_ms();
    if (swo_timestamp > now)
      swo_timestamp = now;
    if ((now - swo_timestamp) > 50) {
      bmp_display(swo_tx_buf, swo_tx_len);
      swo_tx_len = 0;
      swo_timestamp = now;
    }
    return;
  }

  for (int i = 0; i < swo_rx_len; i++) {
    uint8_t ch = swo_rx_buf[i];
    if (swo_pkt_len == 0) {                 /* header */
      uint32_t channel = (uint32_t)ch >> 3; /* channel number */
      uint32_t size = ch & 0x7;             /* drop channel number */
      if (size == 0x01)
        swo_pkt_len = 1; /* SWO packet 0x01XX */
      else if (size == 0x02)
        swo_pkt_len = 2; /* SWO packet 0x02XXXX */
      else if (size == 0x03)
        swo_pkt_len = 4; /* SWO packet 0x03XXXXXXXX */
      swo_print =
          (swo_pkt_len != 0) && ((swo_channelmask & (1UL << channel)) != 0UL);
    } else if (swo_pkt_len <= 4) { /* data */
      if (swo_print) {
        swo_tx_buf[swo_tx_len++] = ch;
        if (swo_tx_len == sizeof(swo_tx_buf)) {
          bmp_display(swo_tx_buf, swo_tx_len);
          swo_timestamp = mp_hal_ticks_ms();
          swo_tx_len = 0;
        }
      }
      --swo_pkt_len;
    } else { /* recover */
      swo_tx_len = 0;
      swo_pkt_len = 0;
    }
  }
  return;
}

/* not truncated */
