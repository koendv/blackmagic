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

/* target power */

#ifdef PWR_EN_PIN
void platform_target_set_power(bool power) {
  if (power)
    mp_hal_pin_high(PWR_EN_PIN);
  else
    mp_hal_pin_low(PWR_EN_PIN);
}

bool platform_target_get_power(void) { return mp_hal_pin_read(PWR_EN_PIN); }
#endif

void platform_init(void) {

  /* pin initialization */
#ifdef LED_PIN
  mp_hal_pin_output(LED_PIN);
#endif
  platform_srst_set_val(0);
  mp_hal_pin_output(SWDIO_PIN);
  mp_hal_pin_config_speed(SWDIO_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
  mp_hal_pin_low(SWDIO_PIN);
#ifdef SWDIO2_PIN
  mp_hal_pin_input(SWDIO2_PIN);
  mp_hal_pin_config_speed(SWDIO2_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
#endif
  mp_hal_pin_output(SWCLK_PIN);
  mp_hal_pin_config_speed(SWCLK_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
  mp_hal_pin_low(SWCLK_PIN);
#ifdef SWCLK2_PIN
  mp_hal_pin_input(SWCLK2_PIN);
  mp_hal_pin_config_speed(SWCLK2_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
#endif
  mp_hal_pin_input(SWO_PIN);
  mp_hal_pin_config_speed(SWO_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
#ifdef SWDIR_PIN
  mp_hal_pin_output(SWDIR_PIN);
  mp_hal_pin_config_speed(SWDIR_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
  mp_hal_pin_high(SWDIR_PIN);
#endif
  mp_hal_pin_output(TDI_PIN);
  mp_hal_pin_config_speed(TDI_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
  mp_hal_pin_low(TDI_PIN);
#ifdef TDI2_PIN
  mp_hal_pin_input(TDI2_PIN);
  mp_hal_pin_config_speed(TDI2_PIN, GPIO_SPEED_FREQ_VERY_HIGH);
#endif
#ifdef VTREF_PIN
  mp_hal_pin_config(VTREF_PIN, MP_HAL_PIN_MODE_ADC, MP_HAL_PIN_PULL_NONE, 0);
#endif
#ifdef PWR_EN_PIN
  mp_hal_pin_output(PWR_EN_PIN);
  mp_hal_pin_low(PWR_EN_PIN);
#endif
#ifdef VUSB_PIN
  mp_hal_pin_config(VUSB_PIN, MP_HAL_PIN_MODE_ADC, MP_HAL_PIN_PULL_NONE, 0);
#endif
#ifdef TRST_PIN
  mp_hal_pin_output(TRST_PIN);
  mp_hal_pin_high(TRST_PIN);
#endif

  /* enable crc hardware */
  generic_crc32_init();

#if defined(PLATFORM_HAS_DEBUG)
  debug_bmp = false;
#endif
}

/* not truncated */
