#include "py/obj.h"
#include "general.h"
#include "platform.h"
#include "mod_bmp.h"
#include "mod_dap.h"
#include "gdb_main.h"
#include "gdb_rxtx.h"
#include "traceswo.h"
#include "tracebuf.h"
#include "gpio.h"
#include "crc32.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "lib/timeutils/timeutils.h"
#include "rtc.h"
#include "factoryreset.h"

#define mp_raise_RuntimeError(msg) (mp_raise_msg(&mp_type_RuntimeError, (msg)))

void platform_delay(uint32_t ms) { mp_hal_delay_ms(ms); }

uint32_t platform_time_ms(void) { return mp_hal_ticks_ms(); }

/* time in s since jan 1, 1970 */
uint32_t unix_time_ms() {
  RTC_DateTypeDef date;
  RTC_TimeTypeDef time;
  rtc_init_finalise();
  HAL_RTC_GetTime(&RTCHandle, &time, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&RTCHandle, &date, RTC_FORMAT_BIN);
  /* TZ=UTC date --date="1/1/2000" "+%s" */
  uint32_t utime =
      timeutils_seconds_since_2000(2000 + date.Year, date.Month, date.Date,
                                   time.Hours, time.Minutes, time.Seconds) +
      946684800;
  return utime;
}

void debug_printf(const char *format, ...) {
  if (debug_bmp) {
    va_list ap;
    va_start(ap, format);
    debug_vprintf(format, ap);
    va_end(ap);
  }
  return;
}

/* debug vprintf */

void debug_vprintf(const char *fmt, va_list ap) {
  char buf[256];
  int len;
  if ((len = vsnprintf(buf, sizeof(buf), fmt, ap)) > 0)
    mp_hal_stdout_tx_strn_cooked(buf, len);
}

void bmp_loop() {
  MICROPY_HW_LED_ON(MICROPY_HW_LED1);

  /* communication with gdb */
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

/* create boot.py, main.py, pybcdc_inf, readme.txt  */

#if MICROPY_HW_ENABLE_STORAGE

#if MICROPY_VFS_FAT

static const char fresh_boot_py[] =
  "# boot.py -- run on boot-up\r\n"
  "# can run arbitrary Python, but best to keep it minimal\r\n"
  "\r\n"
  "import machine, pyb, time, bmp, dap\r\n"
  "pyb.country('US') # ISO 3166-1 Alpha-2 code, eg US, GB, DE, AU\r\n"
  "#pyb.main('main.py') # main script to run after this one\r\n"
#if MICROPY_HW_ENABLE_USB
  "\r\n"
  "if 0:\r\n"
#if MICROPY_HW_USB_HS && !MICROPY_HW_USB_HS_IN_FS
  "  pyb.usb_mode('VCP+HID', vid=0x1d50, pid=0x6018, hid=dap.hid_info, high_speed=True)\r\n"
#else
  "  pyb.usb_mode('VCP+HID', vid=0x1d50, pid=0x6018, hid=dap.hid_info)\r\n"
#endif
  "  time.sleep(1)\r\n"
  "  dap.init()\r\n"
  "  print('dap')\r\n"
  "\r\n"
  "if 0:\r\n"
#if MICROPY_HW_USB_HS && !MICROPY_HW_USB_HS_IN_FS
  "  pyb.usb_mode('VCP+VCP', vid=0x1d50, pid=0x6018, high_speed=True)\r\n"
#else
  "  pyb.usb_mode('VCP+VCP', vid=0x1d50, pid=0x6018)\r\n"
#endif
  "  bmp.init(stream=pyb.USB_VCP(1))\n"
  "  print('bmp usb')\r\n"
#endif
  "\r\n"
  "# not truncated\r\n"
;

static const char fresh_main_py[] =
  "from pye_mp import pye\r\n"
  "import bmp, target, semihosting, dap\r\n"
  "semihosting.init()\r\n"
  "\r\n"
  "def repl_callback(s):\r\n"
  "  return str(eval(s))\r\n"
  "bmp.repl_fun(repl_callback)\r\n"
#if MICROPY_PY_LWIP == 1
  "\r\n"
  "import network\r\n"
  "def wifi_on():\r\n"
  "    import network\r\n"
  "    wlan = network.WLAN(network.STA_IF)\r\n"
  "    wlan.active(True)\r\n"
  "    if not wlan.isconnected():\r\n"
  "        print('connecting...')\r\n"
  "        wlan.connect('essid', 'passwd')\r\n"
  "        while not wlan.isconnected():\r\n"
  "            pass\r\n"
  "    print(wlan.ifconfig())\r\n"
  "\r\n"
  "if 0:\r\n"
  "  wifi_on()\r\n"
  "  bmp.init(tcp=3333)\r\n"
  "  print('bmp tcp')\r\n"
#endif
  "\r\n"
  "# not truncated\r\n"
;

#if MICROPY_HW_ENABLE_USB
static const char fresh_pybcdc_inf[] =
#include "genhdr/pybcdc_inf.h"
;

static const char fresh_readme_txt[] =
    "This is MicroPython with Black Magic Probe and free-dap extensions.\r\n"
    "Compiled " __DATE__ " " __TIME__"\r\n"
;
#endif

typedef struct _factory_file_t {
    const char *name;
    size_t len;
    const char *data;
} factory_file_t;

static const factory_file_t factory_files[] = {
    {"boot.py", sizeof(fresh_boot_py) - 1, fresh_boot_py},
    {"main.py", sizeof(fresh_main_py) - 1, fresh_main_py},
    #if MICROPY_HW_ENABLE_USB
    {"pybcdc.inf", sizeof(fresh_pybcdc_inf) - 1, fresh_pybcdc_inf},
    {"README.txt", sizeof(fresh_readme_txt) - 1, fresh_readme_txt},
    #endif
};

void factory_reset_make_files(FATFS *fatfs) {
    for (int i = 0; i < MP_ARRAY_SIZE(factory_files); ++i) {
        const factory_file_t *f = &factory_files[i];
        FIL fp;
        FRESULT res = f_open(fatfs, &fp, f->name, FA_WRITE | FA_CREATE_ALWAYS);
        if (res == FR_OK) {
            UINT n;
            f_write(&fp, f->data, f->len, &n);
            f_close(&fp);
        }
    }
}

#endif // MICROPY_VFS_FAT

#endif // MICROPY_HW_ENABLE_STORAGE

/* not truncated */
