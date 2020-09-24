#ifndef BMP_GPIO_H
#define BMP_GPIO_H
#include <py/mphal.h>
#include <genhdr/pins.h>

/* split this from platform.h, and include only in jtagtap.c and swdptap.c,
 * else conflict between same pin names in target and platform */

/* jtag connector */
/* VTREF_PIN: target voltage reference */
/* PWR_EN_PIN: target power enable */
/* VUSB_PIN: usb 5.0v voltage measurement */
/* LED_PIN: bicolor led. states: red, green, orange, off */

/* bit-banging */
#define gpio_set(port, pin) (mp_hal_pin_high(pin))
#define gpio_clear(port, pin) (mp_hal_pin_low(pin))
#define gpio_get(port, pin) (mp_hal_pin_read(pin))
#define gpio_set_val(port, pin, val) (mp_hal_pin_write((pin), (val)))

/* micropython pin names */
#define SWDIO_PIN pyb_pin_BMP_SWDIO
#define SWCLK_PIN pyb_pin_BMP_SWCLK
#define SWO_PIN pyb_pin_BMP_SWO
#define SRST_PIN pyb_pin_BMP_SRST

#ifdef pyb_pin_BMP_SWDIR
#define SWDIR_PIN pyb_pin_BMP_SWDIR
#endif

#define TDI_PIN pyb_pin_BMP_TDI

#ifdef pyb_pin_BMP_TRST
#define TRST_PIN pyb_pin_BMP_TRST
#endif

/* shared jtag and swd pins */
#define TMS_PIN SWDIO_PIN
#define TCK_PIN SWCLK_PIN
#define TDO_PIN SWO_PIN

#ifdef pyb_pin_BMP_PWR_EN
#define PWR_EN_PIN pyb_pin_BMP_PWR_EN
#endif

/* XXX scrap ? */

#ifdef pyb_pin_BMP_SWDIO2
#define SWDIO2_PIN pyb_pin_BMP_SWDIO2
#endif


#ifdef pyb_pin_BMP_SWCLK2
#define SWCLK2_PIN pyb_pin_BMP_SWCLK2
#endif

#ifdef pyb_pin_BMP_TDI2
#define TDI2_PIN pyb_pin_BMP_TDI2
#endif


#define TMS_SET_MODE() (mp_hal_pin_output(TMS_PIN))
#ifdef SWDIR_PIN
#define SWDIO_MODE_FLOAT() ({mp_hal_pin_input(SWDIO_PIN);mp_hal_pin_low(SWDIR_PIN);})
#define SWDIO_MODE_DRIVE() ({mp_hal_pin_output(SWDIO_PIN);mp_hal_pin_high(SWDIR_PIN);})
#else
#define SWDIO_MODE_FLOAT() ({mp_hal_pin_input(SWDIO_PIN);})
#define SWDIO_MODE_DRIVE() ({mp_hal_pin_output(SWDIO_PIN);})
#endif

#endif
