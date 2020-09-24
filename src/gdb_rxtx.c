/* 
 * read and write from streams and tcp sockets:
 * USB_VCP, bluetooth HID, UART, tcp socket
 */

/* 
 * Decide whether or not to include tcp/ip support.
 * MICROPY_PY_LWIP is stm32, CONFIG_TCPIP_LWIP is esp32. 
 * Override this by setting BMP_USE_LWIP in mpconfigboard.h
 */

#include "py/mpconfig.h"

#ifndef BMP_USE_LWIP
#if (MICROPY_PY_LWIP == 1) || (CONFIG_TCPIP_LWIP == 1)
#define BMP_USE_LWIP 1
#else
#define BMP_USE_LWIP 0
#endif
#endif

#include "py/obj.h"
#include "py/stream.h"
#include "py/runtime.h"
#include "ctype.h"

#if BMP_USE_LWIP == 1
#include "lwip/ip.h"
#include "lwip/tcp.h"
#endif

#include "general.h"
#include "gdb_rxtx.h"

#define mp_raise_RuntimeError(msg) (mp_raise_msg(&mp_type_RuntimeError, (msg)))

enum gdb_mode_t {
  GDB_IDLE,
  GDB_STREAM,
  GDB_TCP,
};

enum gdb_mode_t gdb_mode;
static mp_obj_t gdb_strm;

/* receive buffer */
static uint8_t gdb_rx_buf[GDB_MAX_PACKET_SIZE];
static uint32_t gdb_rx_head = 0;
static uint32_t gdb_rx_tail = 0;

/* circular transmit buffer */
static uint8_t gdb_tx_buf[2 * GDB_MAX_PACKET_SIZE] = {0};
static uint32_t gdb_tx_head = 0;
static uint32_t gdb_tx_tail = 0;
static bool gdb_tx_flush = false;

#if BMP_USE_LWIP == 1
struct tcp_pcb *gdb_server_pcb;
struct tcp_pcb *gdb_client_pcb;
#endif

static inline uint32_t next_rx(uint32_t p) {
  return (p + 1) % sizeof(gdb_rx_buf);
}
static inline uint32_t next_tx(uint32_t p) {
  return (p + 1) % sizeof(gdb_tx_buf);
}

/*---------------------------------------------------------------------------*/

static uint32_t gdb_stream_read(uint8_t *buf, size_t len) {
  int errcode;
  uint32_t recvd;
  const mp_stream_p_t *stream_p = mp_get_stream(gdb_strm);
  mp_uint_t flags =
      stream_p->ioctl(gdb_strm, MP_STREAM_POLL, MP_STREAM_POLL_RD, &errcode);
  if (flags & MP_STREAM_POLL_RD)
    recvd = stream_p->read(gdb_strm, buf, len, &errcode);
  else
    recvd = 0;
  return recvd;
}

static uint32_t gdb_stream_write(uint8_t *buf, size_t len) {
  int errcode;
  uint32_t sent = 0;
  const mp_stream_p_t *stream_p = mp_get_stream(gdb_strm);
  mp_uint_t flags =
      stream_p->ioctl(gdb_strm, MP_STREAM_POLL, MP_STREAM_POLL_WR, &errcode);
  if (flags & MP_STREAM_POLL_WR)
    sent = stream_p->write(gdb_strm, buf, len, &errcode);
  else
    sent = 0;
  return sent;
}

static void gdb_stream_flush() {
  int errcode;
  const mp_stream_p_t *stream_p = mp_get_stream(gdb_strm);
  mp_uint_t flags = stream_p->ioctl(gdb_strm, MP_STREAM_FLUSH, 0, &errcode);
  (void)flags;
}

/*---------------------------------------------------------------------------*/

#if BMP_USE_LWIP == 1
void gdb_tcp_eof() {
  gdb_client_pcb = NULL;
  gdb_tx_head = 0;
  gdb_tx_tail = 0;
  gdb_rx_head = 0;
  gdb_rx_tail = 0;
  gdb_tx_flush = false;
}

err_t gdb_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  if (p == NULL) {
    // closing connection
    gdb_tcp_eof();
    return ERR_OK;
  }
  tcp_recved(tpcb, p->tot_len);
  if (gdb_client_pcb == NULL) {
    pbuf_free(p);
    return ERR_OK;
  }
  // receive data
  for (struct pbuf *q = p; q != NULL; q = q->next) {
    for (int i = 0; i < q->len; i++) {
      char ch = ((char *)q->payload)[i];
      if (next_rx(gdb_rx_head) != gdb_rx_tail) {
        gdb_rx_buf[gdb_rx_head] = ch;
        gdb_rx_head = next_rx(gdb_rx_head);
      } else
        DEBUG_WARN("gdb_rx_buf overflow");
    }
  }
  pbuf_free(p);
  return ERR_OK;
}

err_t gdb_tcp_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
  // only one connection at a time.
  if (gdb_client_pcb != NULL)
    tcp_abort(gdb_client_pcb); // close old connection first
  gdb_client_pcb = newpcb;
  tcp_recv(gdb_client_pcb, gdb_tcp_recv);
  return ERR_OK;
}

static uint32_t gdb_tcp_write(uint8_t *buf, size_t len) {
  if (gdb_client_pcb == NULL)
    return 0;
  if (tcp_write(gdb_client_pcb, buf, len, TCP_WRITE_FLAG_COPY) == ERR_OK) {
    tcp_output(gdb_client_pcb);
    return len;
  } else
    return 0;
}

static void gdb_tcp_flush() {
  if (gdb_client_pcb != NULL)
    tcp_output(gdb_client_pcb);
}
#endif
/*---------------------------------------------------------------------------*/

static uint32_t gdb_buf_write(uint8_t *buf, size_t len) {
  uint32_t sent;
  switch (gdb_mode) {
  case GDB_IDLE:
    sent = len;
    break;
  case GDB_STREAM:
    sent = gdb_stream_write(buf, len);
    break;
#if BMP_USE_LWIP == 1
  case GDB_TCP:
    sent = gdb_tcp_write(buf, len);
    break;
#endif
  default:
    sent = len;
    break;
  }
  return sent;
}

static void gdb_buf_flush() {
  switch (gdb_mode) {
  case GDB_IDLE:
    break;
  case GDB_STREAM:
    gdb_stream_flush();
    break;
#if BMP_USE_LWIP == 1
  case GDB_TCP:
    gdb_tcp_flush();
    break;
#endif
  default:
    break;
  }
  return;
}

/*---------------------------------------------------------------------------*/

/* call gdb_rx_char with the received characters */

static void gdb_rx() {
  switch (gdb_mode) {
  case GDB_IDLE: {
    break;
  }
  case GDB_STREAM: {
    uint32_t recvd = gdb_stream_read(gdb_rx_buf, sizeof(gdb_rx_buf));
    for (int i = 0; i < recvd; i++)
      gdb_rx_char(gdb_rx_buf[i]);
    break;
  }
#if BMP_USE_LWIP == 1
  case GDB_TCP: {
    while (gdb_rx_tail != gdb_rx_head) {
      gdb_rx_char(gdb_rx_buf[gdb_rx_tail]);
      gdb_rx_tail = next_rx(gdb_rx_tail);
    }
    break;
  }
#endif
  default:
    gdb_mode = GDB_IDLE;
  }
}

/*---------------------------------------------------------------------------*/

void gdb_rxtx_init_stream(mp_obj_t mp_strm) {
  gdb_rxtx_deinit();
  /* check stream valid */
  mp_get_stream_raise(mp_strm, MP_STREAM_OP_READ | MP_STREAM_OP_WRITE |
                                   MP_STREAM_OP_IOCTL);
  gdb_strm = mp_strm;
  gdb_mode = GDB_STREAM;
}

#if BMP_USE_LWIP == 1
void gdb_rxtx_init_tcp(mp_int_t gdb_tcp_port) {
  gdb_rxtx_deinit();
  if ((gdb_tcp_port < 1) || (gdb_tcp_port > 65535))
    mp_raise_ValueError(MP_ERROR_TEXT("tcp port"));
  gdb_server_pcb = tcp_new();
  if (gdb_server_pcb == NULL)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no memory"));
  if (tcp_bind(gdb_server_pcb, IP4_ADDR_ANY, gdb_tcp_port) != ERR_OK)
    mp_raise_RuntimeError(MP_ERROR_TEXT("port in use"));
  gdb_server_pcb = tcp_listen(gdb_server_pcb);
  if (gdb_server_pcb == NULL)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no memory"));
  // tcp_arg(gdb_server_pcb, NULL);
  tcp_accept(gdb_server_pcb, gdb_tcp_accept);
  gdb_mode = GDB_TCP;
  return;
}
#else
void gdb_rxtx_init_tcp(mp_int_t gdb_tcp_port) {
  (void) gdb_tcp_port;
  mp_raise_RuntimeError(MP_ERROR_TEXT("no ip"));
}
#endif

void gdb_rxtx_deinit() {
#if BMP_USE_LWIP == 1
  /* XXX also: switch tcp callbacks off */
  if ((gdb_mode == GDB_TCP) && (gdb_client_pcb != NULL))
    tcp_abort(gdb_client_pcb);
  gdb_client_pcb = NULL;
  if ((gdb_mode == GDB_TCP) && (gdb_server_pcb != NULL))
    tcp_close(gdb_server_pcb);
  gdb_server_pcb = NULL;
#endif
  gdb_mode = GDB_IDLE;
  gdb_strm = mp_const_none;
  gdb_tx_head = 0;
  gdb_tx_tail = 0;
  gdb_rx_head = 0;
  gdb_rx_tail = 0;
  gdb_tx_flush = false;
}

/*---------------------------------------------------------------------------*/

static void gdb_tx_packet() {
  uint32_t len;
  while (gdb_tx_tail != gdb_tx_head) {
    if (gdb_tx_head > gdb_tx_tail)
      len = gdb_tx_head - gdb_tx_tail;
    else
      len = sizeof(gdb_tx_buf) - gdb_tx_tail;
    uint32_t chars_sent = gdb_buf_write(gdb_tx_buf + gdb_tx_tail, len);
    gdb_tx_tail = (gdb_tx_tail + chars_sent) % sizeof(gdb_tx_buf);
  }
  gdb_buf_flush();
  gdb_tx_flush = false;
  return;
}

void gdb_tx_char(uint8_t ch, bool flush) {
  DEBUG_INFO("%s: ch %c (%02X)", __func__, isprint(ch) ? ch : ' ', ch);
  if (next_tx(gdb_tx_head) != gdb_tx_tail) {
    gdb_tx_buf[gdb_tx_head] = ch;
    gdb_tx_head = next_tx(gdb_tx_head);
  } else
    // ought never to happen - increase gdb_tx_buf
    DEBUG_WARN("gdb_tx_buf overflow\n");
  gdb_tx_flush |= flush || (next_tx(gdb_tx_head) == gdb_tx_tail);
};

/*---------------------------------------------------------------------------*/

void gdb_rxtx() {
  gdb_rx();
  if (gdb_tx_flush)
    gdb_tx_packet();
};

// not truncated
