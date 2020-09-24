#include "general.h"
#include "platform.h"
#include "crc32.h"
#include "py/mphal.h"
#include "gpio.h"

/* target reset */

void platform_srst_set_val(bool assert) {
  if (assert) {
    mp_hal_pin_open_drain(SRST_PIN);
    mp_hal_pin_low(SRST_PIN);
  } else {
    mp_hal_pin_high(SRST_PIN);
    mp_hal_pin_input(SRST_PIN);
  }
}

bool platform_srst_get_val() { return (mp_hal_pin_read(SRST_PIN) == 0); }

void platform_init(void) {

  /* pin initialization */
#ifdef LED_PIN
  mp_hal_pin_output(LED_PIN);
#endif
  platform_srst_set_val(0);
  mp_hal_pin_output(SWDIO_PIN);
  mp_hal_pin_config_speed(SWDIO_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
  mp_hal_pin_low(SWDIO_PIN);
  mp_hal_pin_output(SWCLK_PIN);
  mp_hal_pin_config_speed(SWCLK_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
  mp_hal_pin_low(SWCLK_PIN);
  mp_hal_pin_input(SWO_PIN);
  mp_hal_pin_config_speed(SWO_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
  mp_hal_pin_output(TDI_PIN);
  mp_hal_pin_config_speed(TDI_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
  mp_hal_pin_low(TDI_PIN);

  /* enable crc hardware */
  generic_crc32_init();

#if defined(PLATFORM_HAS_DEBUG)
  debug_bmp = false;
#endif
}

/* not truncated */
