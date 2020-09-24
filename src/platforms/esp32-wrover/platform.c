#include "general.h"
#include "platform.h"
#include "py/mphal.h"
#include "sys/time.h"
#include "mphalport.h"
#include "gpio.h"
#include "crc32.h"
#include "mod_bmp.h"

#include "driver/uart.h"

bool bmp_idle;
bool debug_bmp;

/* target reset */

void platform_srst_set_val(bool assert) {
  if (assert) {
    mp_hal_pin_open_drain(SRST_PIN);
    mp_hal_pin_od_low(SRST_PIN);
  } else {
    mp_hal_pin_high(SRST_PIN);
    mp_hal_pin_input(SRST_PIN);
  }
}

bool platform_srst_get_val() { return (mp_hal_pin_read(SRST_PIN) == 0); }

/* target power */

#ifdef PLATFORM_HAS_POWER_SWITCH

void platform_target_set_power(bool power) {
  if (power)
    mp_hal_pin_high(PWR_EN_PIN);
  else
    mp_hal_pin_low(PWR_EN_PIN);
}

bool platform_target_get_power(void) { return mp_hal_pin_read(PWR_EN_PIN); }

#endif

/*
 * bmp initialization
 */

void platform_init() {
  /* pin initialization */
#ifdef MICROPY_HW_LED1
  mp_hal_pin_output(MICROPY_HW_LED1);
#endif
  platform_srst_set_val(0);
  mp_hal_pin_output(SWDIO_PIN);
  mp_hal_pin_low(SWDIO_PIN);
  mp_hal_pin_output(SWCLK_PIN);
  mp_hal_pin_low(SWCLK_PIN);
  mp_hal_pin_output(TDI_PIN);
  mp_hal_pin_low(TDI_PIN);

  /* enable crc hardware */
  generic_crc32_init();

  debug_bmp = false;
}

/* not truncated */
