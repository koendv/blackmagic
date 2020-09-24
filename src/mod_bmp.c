#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "py/runtime.h"
#include "py/stream.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/mphal.h"

#include "mod_bmp.h"
#include "gdb_packet.h"
#include "gdb_rxtx.h"
#include "exception.h"
#include "morse.h"
#include "traceswo.h"
#include "tracebuf.h"

#define mp_raise_RuntimeError(msg) (mp_raise_msg(&mp_type_RuntimeError, (msg)))

bool bmp_enabled = false;

mp_obj_t *bmp_display_callback_obj = NULL;
mp_obj_t *bmp_repl_callback_obj = NULL;

STATIC mp_obj_t bmp_help();
STATIC mp_obj_t bmp_init(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs);
STATIC mp_obj_t bmp_deinit();
STATIC mp_obj_t bmp_traceswo(size_t n_args, const mp_obj_t *args,
                             mp_map_t *kwargs);
STATIC mp_obj_t bmp_tracebuf(size_t n_args, const mp_obj_t *args,
                             mp_map_t *kwargs);
STATIC mp_obj_t bmp_disp_fun(mp_obj_t callback_fun_obj);
STATIC mp_obj_t bmp_repl_fun(mp_obj_t eval_fun_obj);
STATIC mp_obj_t bmp_debug();

STATIC MP_DEFINE_CONST_FUN_OBJ_0(bmp_help_obj, bmp_help);
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bmp_init_obj, 0, bmp_init);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(bmp_deinit_obj, bmp_deinit);
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bmp_traceswo_obj, 0, bmp_traceswo);
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bmp_tracebuf_obj, 0, bmp_tracebuf);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bmp_disp_fun_obj, bmp_disp_fun);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bmp_repl_fun_obj, bmp_repl_fun);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(bmp_debug_obj, bmp_debug);

STATIC const mp_rom_map_elem_t bmp_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_bmp)},
    {MP_ROM_QSTR(MP_QSTR_help), MP_ROM_PTR(&bmp_help_obj)},
    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&bmp_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&bmp_deinit_obj)},
    {MP_ROM_QSTR(MP_QSTR_traceswo), MP_ROM_PTR(&bmp_traceswo_obj)},
    {MP_ROM_QSTR(MP_QSTR_tracebuf), MP_ROM_PTR(&bmp_tracebuf_obj)},
    {MP_ROM_QSTR(MP_QSTR_disp_fun), MP_ROM_PTR(&bmp_disp_fun_obj)},
    {MP_ROM_QSTR(MP_QSTR_repl_fun), MP_ROM_PTR(&bmp_repl_fun_obj)},
    {MP_ROM_QSTR(MP_QSTR_debug), MP_ROM_PTR(&bmp_debug_obj)},
};

STATIC MP_DEFINE_CONST_DICT(bmp_module_globals, bmp_module_globals_table);

// module object
const mp_obj_module_t mp_module_bmp = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&bmp_module_globals,
};

// Register the module to make it available in Python
MP_REGISTER_MODULE(MP_QSTR_bmp, mp_module_bmp, (MODULE_BMP_ENABLED));

/*
 * help text, created at compile time.
 */

/* put a quote around pin names */

#define STR(str) #str
#define XSTR(str) STR(str)

/* Print pin numbers on esp32 platform */

#ifdef BMP_SWCLK
#define HLP_SWCLK " swclk " XSTR(BMP_SWCLK)
#else
#define HLP_SWCLK ""
#endif
#ifdef BMP_SWDIO
#define HLP_SWDIO " swdio " XSTR(BMP_SWDIO)
#else
#define HLP_SWDIO ""
#endif
#ifdef BMP_SWO
#define HLP_SWO " swo " XSTR(BMP_SWO)
#else
#define HLP_SWO ""
#endif
#ifdef BMP_SRST
#define HLP_SRST " srst " XSTR(BMP_SRST)
#else
#define HLP_SRST ""
#endif
#ifdef BMP_TDI
#define HLP_TDI " tdi " XSTR(BMP_TDI)
#else
#define HLP_TDI ""
#endif
#ifdef BMP_TRST
#define HLP_TRST " trst " XSTR(BMP_TRST)
#else
#define HLP_TRST ""
#endif
#ifdef BMP_VTREF
#define HLP_VTREF " vtref " XSTR(BMP_VTREF)
#else
#define HLP_VTREF ""
#endif

/* help text */

const char bmp_help_text[] =
    "bmp functions:\n"
    " init(stream=|tcp=) -- start bmp on a micropython stream or a tcp port.\n"
    " traceswo() -- display swo logging\n"
    " tracebuf() -- display target buffer\n"
    " disp_fun() -- register callback function to display text\n"
    " repl_fun() -- register callback function to execute python\n"
    " debug()    -- toggle debugging of the probe itself\n"
    HLP_SWCLK HLP_SWDIO HLP_SWO HLP_SRST HLP_TDI HLP_TRST HLP_VTREF "\n"
    "compiled " __DATE__ " " __TIME__ "\n";

/*
 * print help text
 */

STATIC mp_obj_t bmp_help() {
  mp_print_str(MP_PYTHON_PRINTER, bmp_help_text);
  return mp_const_none;
}

/*
 * traceswo.
 * two parameters:
 * uart = serial port
 * chan = is list of swo channels to print.
 * eg. traceswo(uart=machine.UART(1, 2250000), chan=[1,2,3])
 * with no parameters switches traceswo off. 
 */


STATIC mp_obj_t bmp_traceswo(size_t n_args, const mp_obj_t *pos_args,
                             mp_map_t *kw_args) {

  /* parse arguments */
  static const mp_arg_t allowed_args[] = {
      {MP_QSTR_uart, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none}},
      {MP_QSTR_chan, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none}},
  };
  mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
  mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args),
                   allowed_args, args);
  mp_obj_t uart_obj = args[0].u_obj;
  mp_obj_t channel_list_obj = args[1].u_obj;

  /* no arguments: switch swo off */
  if ((uart_obj == mp_const_none) && (channel_list_obj == mp_const_none)) {
    swo_enabled = false;
    swo_stream =  NULL;
    swo_channelmask = 0;
    return mp_const_none;
  }

  swo_enabled = false;

  if (uart_obj != mp_const_none) {
    /* check is stream */
    swo_stream =  NULL;
    swo_stream = mp_get_stream(uart_obj);
    swo_uart = uart_obj;
    swo_channelmask = 0xffffffff;
  }

  if (channel_list_obj != mp_const_none) {
    swo_channelmask = 0;
    if (!mp_obj_is_type(MP_OBJ_FROM_PTR(channel_list_obj), &mp_type_list)) {
      mp_raise_ValueError(MP_ERROR_TEXT("expected channel list"));
    }
    mp_obj_list_t *channel_list = MP_OBJ_TO_PTR(channel_list_obj);
    for (unsigned i = 0; i < channel_list->len; i++) {
      mp_int_t channel = mp_obj_get_int(channel_list->items[i]);
      if ((channel >= 0) && (channel <= 31))
        swo_channelmask |= (uint32_t)0x1 << channel;
    }
  }

  swo_enabled = (swo_stream != NULL) && (swo_channelmask != 0);

  return mp_const_none;
}

/*
 * tracebuf.
 * two parameters:
 * cb is a list of 4 ints: [buf_address, buf_size, head_address, tail_address]
 * ena is bool, default value: True.
 */

STATIC mp_obj_t bmp_tracebuf(size_t n_args, const mp_obj_t *pos_args,
                             mp_map_t *kw_args) {

  /* parse arguments */
  static const mp_arg_t allowed_args[] = {
      {MP_QSTR_cb, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none}},
      {MP_QSTR_ena, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none}},
  };
  mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
  mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args),
                   allowed_args, args);
  mp_obj_t cb_obj = args[0].u_obj;
  mp_obj_t ena_obj = args[1].u_obj;

  if (cb_obj != mp_const_none) {
    if (!mp_obj_is_type(MP_OBJ_FROM_PTR(cb_obj), &mp_type_list))
      mp_raise_TypeError(MP_ERROR_TEXT("expected list"));
    mp_obj_list_t *cb = MP_OBJ_TO_PTR(cb_obj);
    if (cb->len != 4)
      mp_raise_TypeError(
          MP_ERROR_TEXT("expected [buf_addr, buf_size, head_addr, tail_addr]"));
    uint32_t buf_addr = mp_obj_get_int(cb->items[0]);
    uint32_t buf_size = mp_obj_get_int(cb->items[1]);
    uint32_t head_addr = mp_obj_get_int(cb->items[2]);
    uint32_t tail_addr = mp_obj_get_int(cb->items[3]);
    tracebuf_set(buf_addr, buf_size, head_addr, tail_addr);
    tracebuf_enabled = true;
  }
  if (ena_obj != mp_const_none)
    tracebuf_enabled = mp_obj_is_true(ena_obj);

  return mp_const_none;
}

// not truncated
/*
 * bmp.disp_fun() - store callback function to display console message
 */

STATIC mp_obj_t bmp_disp_fun(mp_obj_t callback_fun_obj) {
  bmp_display_callback_obj = MP_OBJ_FROM_PTR(callback_fun_obj);
  return mp_const_none;
}

/*
 * bmp_display(char *)
 * bmp_display() is called by bmp when a message needs to be displayed.
 * bmp_display() calls the callback function, previously registered by
 * bmp_disp_fun(), with the error message as argument. if there is no callback
 * function registered, bmp_display() just prints the message on stderr.
 */

void bmp_display(const uint8_t *msg, size_t msg_len) {
  if (bmp_display_callback_obj == NULL) {
    if (msg && msg_len)
      mp_hal_stdout_tx_strn_cooked((const char *)msg, msg_len);
    return;
  }
  mp_obj_t fn_arg0 = mp_const_empty_bytes;
  if (msg)
    //    fn_arg0 = mp_obj_new_bytearray_by_ref(msg_len, (void*)msg); // XXX
    //    faster?
    fn_arg0 = mp_obj_new_bytearray(msg_len, (void *)msg);
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_call_function_1(bmp_display_callback_obj, fn_arg0);
    nlr_pop();
  } else
    mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
  return;
}

/*
 * bmp.repl_fun()
 * store the callback function to execute gdb "monitor python" commands
 */

STATIC mp_obj_t bmp_repl_fun(mp_obj_t eval_fun_obj) {
  bmp_repl_callback_obj = MP_OBJ_FROM_PTR(eval_fun_obj);
  return mp_const_none;
}

/*
 * bmp_repl() is called by bmp to execute a gdb "monitor python" command.
 * bmp_repl() calls the micropython function, previously registered with
 * bmp.repl_fun(), with the python command as argument. If micropython returns a
 * string, the string is sent to the gdb console.
 */

int bmp_repl(const char *cmd) {
  if (bmp_repl_callback_obj == NULL) {
    gdb_out("repl_fun() not registered\n");
    return 0;
  }
  mp_obj_t fn_arg0 = mp_obj_new_str(cmd, strlen(cmd));
  nlr_buf_t nlr;
  mp_obj_t ret_obj;
  if (nlr_push(&nlr) == 0) {
    ret_obj = mp_call_function_1(bmp_repl_callback_obj, fn_arg0);
    if (mp_obj_is_type(MP_OBJ_FROM_PTR(ret_obj), &mp_type_str) ||
        mp_obj_is_type(MP_OBJ_FROM_PTR(ret_obj), &mp_type_bytes)) {
      mp_obj_str_t *str = MP_OBJ_FROM_PTR(ret_obj);
      gdb_out((const char *)str->data);
      gdb_out("\n");
    }
    nlr_pop();
  } else {
    mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
    return 1;
  }
  return 0;
}

/* bmp.debug -- toggle bmp debugging */

STATIC mp_obj_t bmp_debug() {
  debug_bmp = !debug_bmp;
  return mp_obj_new_bool(debug_bmp);
}

STATIC mp_obj_t bmp_init(size_t n_args, const mp_obj_t *pos_args,
                         mp_map_t *kw_args) {
  /* parse arguments */
  static const mp_arg_t allowed_args[] = {
      {MP_QSTR_stream, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none}},
      {MP_QSTR_tcp, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0}},
  };
  mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
  mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args),
                   allowed_args, args);
  mp_obj_t stream_obj = args[0].u_obj;
  mp_int_t tcp_port = args[1].u_int;

  /* initialize pins */
  platform_init();

  /* initialize gdb port */
  bmp_enabled = false;
  if (tcp_port != 0)
    gdb_rxtx_init_tcp(tcp_port);
  else if (stream_obj != mp_const_none)
    gdb_rxtx_init_stream(stream_obj);
  else
    mp_raise_ValueError(MP_ERROR_TEXT("expected tcp= or stream="));
  /* start bmp loop */
  bmp_enabled = true;
  return mp_const_none;
}

STATIC mp_obj_t bmp_deinit() {
  bmp_enabled = false;
  gdb_rxtx_deinit();
  return mp_const_none;
}

// not truncated
