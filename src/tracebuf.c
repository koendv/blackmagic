#include "py/obj.h"

#include "general.h"
#include "gdb_main.h"
#include "target.h"
#include "target/target_internal.h"
#include "mod_bmp.h"
#include "tracebuf.h"
#include "py/mphal.h"
#include "py/nlr.h"
#include "py/runtime.h"

bool tracebuf_enabled;

static uint32_t buf_addr = 0;
static uint32_t buf_size = 0;
static uint32_t head_addr = 0;
static uint32_t tail_addr = 0;

void tracebuf_set(uint32_t new_buf_addr, uint32_t new_buf_size,
                  uint32_t new_head_addr, uint32_t new_tail_addr) {
  buf_addr = new_buf_addr;
  buf_size = new_buf_size;
  head_addr = new_head_addr;
  tail_addr = new_tail_addr;
}

void tracebuf() {
  uint32_t head;
  uint32_t tail;

  if ((buf_addr != 0) && !cur_target) {
    buf_addr = 0;
    buf_size = 0;
    head_addr = 0;
    tail_addr = 0;
    mp_hal_stdout_tx_str("\r\ntracebuf: lost\r\n");
    return;
  }

  if (buf_addr == 0)
    return;

  if (target_mem_read(cur_target, &head, head_addr, sizeof(head))) {
    buf_addr = 0;
    tracebuf_enabled = false;
    mp_hal_stdout_tx_str("\r\ntracebuf: head read fail\r\n");
    return;
  }

  if (target_mem_read(cur_target, &tail, tail_addr, sizeof(tail))) {
    buf_addr = 0;
    tracebuf_enabled = false;
    mp_hal_stdout_tx_str("\r\ntracebuf: tail read fail\r\n");
    return;
  }

  if (head >= buf_size) {
    buf_addr = 0;
    tracebuf_enabled = false;
    mp_hal_stdout_tx_str("\r\ntracebuf: head out of range error\r\n");
    return;
  }
  if (tail >= buf_size) {
    buf_addr = 0;
    tracebuf_enabled = false;
    mp_hal_stdout_tx_str("\r\ntracebuf: tail out of range error\r\n");
    return;
  }

  if (head == tail)
    return;
  // mp_printf(&mp_plat_print, "\r\ntracebuf: head: %d tail: %d\r\n", head, tail);
  uint32_t len;
  char rd_buf[buf_size];
  if (head > tail) {
    len = head - tail;
    if (target_mem_read(cur_target, rd_buf, buf_addr + tail, len)) {
      buf_addr = 0;
      tracebuf_enabled = false;
      mp_hal_stdout_tx_str("\r\ntracebuf: read fail\r\n");
      return;
    }
  } else {
    len = buf_size - tail;
    if (target_mem_read(cur_target, rd_buf, buf_addr + tail, len)) {
      buf_addr = 0;
      tracebuf_enabled = false;
      mp_hal_stdout_tx_str("\r\ntracebuf: read fail\r\n");
      return;
    }
    if (head && target_mem_read(cur_target, rd_buf + len, buf_addr, head)) {
      buf_addr = 0;
      tracebuf_enabled = false;
      mp_hal_stdout_tx_str("\r\ntracebuf: read fail\r\n");
      return;
    }
    len += head;
  }

  bmp_display((const uint8_t *)rd_buf, len);
  tail = head;
  if (target_mem_write(cur_target, tail_addr, &tail, sizeof(tail))) {
    buf_addr = 0;
    tracebuf_enabled = false;
    mp_hal_stdout_tx_str("\r\ntracebuf: tail write fail\r\n");
    return;
  }
  return;
}
