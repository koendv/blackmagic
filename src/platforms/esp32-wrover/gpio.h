#ifndef BMP_GPIO_H
#define BMP_GPIO_H
#include <py/mphal.h>

/* split this from platform.h, and include only in jtagtap.c and swdptap.c,
 * else conflict between same pin names in target and platform */

/* jtag connector */
/* VTREF_PIN: target voltage reference */
/* PWR_EN_PIN: target power enable */
/* SWDIR_PIN: level translator direction */

/* bit-banging */

#define mp_hal_pin_low(pin) (mp_hal_pin_write((pin), 0))
#define mp_hal_pin_high(pin) (mp_hal_pin_write((pin), 1))

#define gpio_set(port, pin) (mp_hal_pin_high((pin)))
#define gpio_clear(port, pin) (mp_hal_pin_low((pin)))
#define gpio_get(port, pin) (mp_hal_pin_read((pin)))
#define gpio_set_val(port, pin, val) (mp_hal_pin_write((pin), (val)))

/* micropython pin names */
#define SWDIO_PIN BMP_SWDIO
#define SWCLK_PIN BMP_SWCLK
#define SWO_PIN BMP_SWO
#define SWDIR_PIN BMP_SWDIR
#define TDI_PIN BMP_TDI
#define SRST_PIN BMP_SRST
#define TRST_PIN BMP_TRST
#define VTREF_PIN BMP_VTREF
#define PWR_EN_PIN BMP_PWR_EN

/* shared jtag and swd pins */
#define TMS_PIN SWDIO_PIN
#define TCK_PIN SWCLK_PIN
#define TDO_PIN SWO_PIN

#define TMS_SET_MODE() (mp_hal_pin_output(TMS_PIN))
#define SWDIO_MODE_FLOAT() ({mp_hal_pin_input(SWDIO_PIN);mp_hal_pin_low(SWDIR_PIN);})
#define SWDIO_MODE_DRIVE() ({mp_hal_pin_output(SWDIO_PIN);mp_hal_pin_high(SWDIR_PIN);})

#endif
