#include "general.h"
#include "platform.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "sys/time.h"
#include "mphalport.h"
#include "mod_bmp.h"
#include "driver/uart.h"
#include "gdb_main.h"
#include "gdb_rxtx.h"
#include "traceswo.h"
#include "tracebuf.h"
#include "gpio.h"

#define mp_raise_RuntimeError(msg) (mp_raise_msg(&mp_type_RuntimeError, (msg)))

void platform_delay(uint32_t ms) { mp_hal_delay_ms(ms); }

/* time in ms since boot */
uint32_t platform_time_ms() { return mp_hal_ticks_ms(); }

/* time in ms since jan 1, 1970 */
uint32_t unix_time_ms() {
  uint32_t seconds = 0;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  seconds = tv.tv_sec;
  return seconds;
}

/*
 * debug printf
 */

void debug_printf(const char *format, ...) {
  if (debug_bmp) {
    va_list ap;
    va_start(ap, format);
    debug_vprintf(format, ap);
    va_end(ap);
  }
  return;
}

/*
 * debug vprintf
 */

void debug_vprintf(const char *fmt, va_list ap) {
  char buf[256];
  int len;
  if ((len = vsnprintf(buf, sizeof(buf), fmt, ap)) > 0)
    mp_hal_stdout_tx_strn_cooked(buf, len);
}

void bmp_loop() {

  MICROPY_HW_LED_ON(MICROPY_HW_LED1);
  /* receive */
  if (bmp_enabled) {
    gdb_rxtx();
    if (gdb_target_running)
      gdb_poll_target();
    }

  if (swo_enabled)
    traceswo_decode();

  if (tracebuf_enabled)
    tracebuf();

  // blink led every 8 seconds
  if (((mp_hal_ticks_ms() >> 8) & 0x1F) != 0)
    MICROPY_HW_LED_OFF(MICROPY_HW_LED1);
}

/* not truncated */
