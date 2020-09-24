#include "mod_bmp.h"
#include "target.h"
#include "exception.h"
#include "gdb_main.h"
#include "target/target_internal.h"
#include "target/jtag_scan.h"

#include "py/nlr.h"
#include "py/obj.h"
#include "py/objarray.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/binary.h"
#include "py/mpconfig.h"

#define mp_raise_RuntimeError(msg) (mp_raise_msg(&mp_type_RuntimeError, (msg)))

const char target_help_text[] =
    "scan target:\n"
    " jtag_scan(assert_srst: bool, ir_len: list)\n"
    " swdp_scan(assert_srst: bool) \n"
    "attach/deattach target: \n"
    " attach(n:int)\n"
    " detach()\n"
    " reattach()\n"
    " attached()\n"
    " driver_name()\n"
    " core_name()\n"
    " check_error()\n"
    "memory access:\n"
    " mem_map()\n"
    " mem_map_xml()\n"
    " mem_read(dst: bytearray, src: int)\n"
    " mem_write(dst: int, src: bytearray)\n"
    "flash memory access:\n"
    " flash_erase(addr: int, len: int)\n"
    " flash_write(dst: int, src: bytearray)\n"
    " flash_done()\n"
    "register access:\n"
    " tdesc_xml()\n"
    " regs_size()\n"
    " regs_read(data: bytearray)\n"
    " regs_write(data: bytearray)\n"
    " reg_read(reg: int, data: bytearray)\n"
    " reg_write(reg: int, data: bytearray)\n"
    "halt/resume:\n"
    " reset() -- toggle reset pin\n"
    " halt_request()\n"
    " halt_poll() -- returns tuple (halt_code: int, halt_reason: str, "
    "watchpoint_address: int)\n"
    " halt_resume(step: bool)\n"
    "semihosting:\n"
    " cmdline(cmdline: str) -- set command line args\n"
    " heapsize(heapsize: list) -- set heap/stack in crt0.S\n"
    "break/watchpoint:\n"
    " breakwatch_set(bw_type: int, addr: int, len: int)\n"
    " breakwatch_clear(bw_type: int, addr: int, len: int)\n"
    "halt_poll() return value:\n"
    " HALT_RUNNING -- target not halted\n"
    " HALT_ERROR -- failed to read target status\n"
    " HALT_REQUEST\n"
    " HALT_STEPPING\n"
    " HALT_BREAKPOINT\n"
    " HALT_WATCHPOINT\n"
    " HALT_FAULT\n"
    "breakpoint/watchpoint type:\n"
    " BREAK_SOFT\n"
    " BREAK_HARD\n"
    " WATCH_WRITE\n"
    " WATCH_READ\n"
    " WATCH_ACCESS\n";

STATIC mp_obj_t mp_target_help();
/* scanning */
STATIC mp_obj_t mp_target_jtag_scan(mp_obj_t assert_srst_obj,
                                    mp_obj_t ir_len_obj);
STATIC mp_obj_t mp_target_swdp_scan(mp_obj_t assert_srst_obj);

/* attach and deattach */
STATIC mp_obj_t mp_target_attach(mp_obj_t n_obj);
STATIC mp_obj_t mp_target_detach();
STATIC mp_obj_t mp_target_reattach();
STATIC mp_obj_t mp_target_attached();
STATIC mp_obj_t mp_target_driver_name();
STATIC mp_obj_t mp_target_core_name();
STATIC mp_obj_t mp_target_check_error();
/* memory access */
STATIC mp_obj_t mp_target_mem_map();
STATIC mp_obj_t mp_target_mem_map_xml();
STATIC mp_obj_t mp_target_mem_read(mp_obj_t dst_obj, mp_obj_t src_obj);
STATIC mp_obj_t mp_target_mem_write(mp_obj_t dst_obj, mp_obj_t src_obj);
/* flash access */
STATIC mp_obj_t mp_target_flash_erase(mp_obj_t addr_obj, mp_obj_t len_obj);
STATIC mp_obj_t mp_target_flash_write(mp_obj_t dst_obj, mp_obj_t src_obj);
STATIC mp_obj_t mp_target_flash_done();
/* register access */
STATIC mp_obj_t mp_target_tdesc_xml();
STATIC mp_obj_t mp_target_regs_size();
STATIC mp_obj_t mp_target_regs_read(mp_obj_t data_obj);
STATIC mp_obj_t mp_target_regs_write(mp_obj_t data_obj);
STATIC mp_obj_t mp_target_reg_read(mp_obj_t reg_obj, mp_obj_t data_obj);
STATIC mp_obj_t mp_target_reg_write(mp_obj_t reg_obj, mp_obj_t data_obj);
/* halt/step/resume */
STATIC mp_obj_t mp_target_reset();
STATIC mp_obj_t mp_target_halt_request();
STATIC mp_obj_t mp_target_halt_poll();
STATIC mp_obj_t mp_target_halt_resume(mp_obj_t step_obj);
/* semihosting */
STATIC mp_obj_t mp_target_cmdline(mp_obj_t cmdline_obj);
STATIC mp_obj_t mp_target_heapsize(mp_obj_t heapsize_obj);
/* break/watchpoint */
STATIC mp_obj_t mp_target_breakwatch_set(mp_obj_t bw_type_obj,
                                         mp_obj_t addr_obj, mp_obj_t len_obj);
STATIC mp_obj_t mp_target_breakwatch_clear(mp_obj_t bw_type_obj,
                                           mp_obj_t addr_obj, mp_obj_t len_obj);

STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_help_obj, mp_target_help);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_target_jtag_scan_obj, mp_target_jtag_scan);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_target_swdp_scan_obj, mp_target_swdp_scan);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_target_attach_obj, mp_target_attach);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_detach_obj, mp_target_detach);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_reattach_obj, mp_target_reattach);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_attached_obj, mp_target_attached);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_driver_name_obj,
                                 mp_target_driver_name);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_core_name_obj, mp_target_core_name);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_check_error_obj,
                                 mp_target_check_error);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_mem_map_obj, mp_target_mem_map);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_mem_map_xml_obj,
                                 mp_target_mem_map_xml);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_target_mem_read_obj, mp_target_mem_read);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_target_mem_write_obj, mp_target_mem_write);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_target_flash_erase_obj,
                                 mp_target_flash_erase);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_target_flash_write_obj,
                                 mp_target_flash_write);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_flash_done_obj,
                                 mp_target_flash_done);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_tdesc_xml_obj, mp_target_tdesc_xml);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_regs_size_obj, mp_target_regs_size);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_target_regs_read_obj, mp_target_regs_read);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_target_regs_write_obj,
                                 mp_target_regs_write);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_target_reg_read_obj, mp_target_reg_read);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_target_reg_write_obj, mp_target_reg_write);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_reset_obj, mp_target_reset);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_halt_request_obj,
                                 mp_target_halt_request);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_target_halt_poll_obj, mp_target_halt_poll);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_target_halt_resume_obj,
                                 mp_target_halt_resume);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_target_cmdline_obj, mp_target_cmdline);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_target_heapsize_obj, mp_target_heapsize);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mp_target_breakwatch_set_obj,
                                 mp_target_breakwatch_set);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mp_target_breakwatch_clear_obj,
                                 mp_target_breakwatch_clear);

STATIC const mp_rom_map_elem_t target_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_target)},
    {MP_ROM_QSTR(MP_QSTR_help), MP_ROM_PTR(&mp_target_help_obj)},
    {MP_ROM_QSTR(MP_QSTR_jtag_scan), MP_ROM_PTR(&mp_target_jtag_scan_obj)},
    {MP_ROM_QSTR(MP_QSTR_swdp_scan), MP_ROM_PTR(&mp_target_swdp_scan_obj)},
    {MP_ROM_QSTR(MP_QSTR_attach), MP_ROM_PTR(&mp_target_attach_obj)},
    {MP_ROM_QSTR(MP_QSTR_detach), MP_ROM_PTR(&mp_target_detach_obj)},
    {MP_ROM_QSTR(MP_QSTR_reattach), MP_ROM_PTR(&mp_target_reattach_obj)},
    {MP_ROM_QSTR(MP_QSTR_attached), MP_ROM_PTR(&mp_target_attached_obj)},
    {MP_ROM_QSTR(MP_QSTR_driver_name), MP_ROM_PTR(&mp_target_driver_name_obj)},
    {MP_ROM_QSTR(MP_QSTR_core_name), MP_ROM_PTR(&mp_target_core_name_obj)},
    {MP_ROM_QSTR(MP_QSTR_check_error), MP_ROM_PTR(&mp_target_check_error_obj)},
    {MP_ROM_QSTR(MP_QSTR_mem_map), MP_ROM_PTR(&mp_target_mem_map_obj)},
    {MP_ROM_QSTR(MP_QSTR_mem_map_xml), MP_ROM_PTR(&mp_target_mem_map_xml_obj)},
    {MP_ROM_QSTR(MP_QSTR_mem_read), MP_ROM_PTR(&mp_target_mem_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_mem_write), MP_ROM_PTR(&mp_target_mem_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_flash_erase), MP_ROM_PTR(&mp_target_flash_erase_obj)},
    {MP_ROM_QSTR(MP_QSTR_flash_write), MP_ROM_PTR(&mp_target_flash_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_flash_done), MP_ROM_PTR(&mp_target_flash_done_obj)},
    {MP_ROM_QSTR(MP_QSTR_tdesc_xml), MP_ROM_PTR(&mp_target_tdesc_xml_obj)},
    {MP_ROM_QSTR(MP_QSTR_regs_size), MP_ROM_PTR(&mp_target_regs_size_obj)},
    {MP_ROM_QSTR(MP_QSTR_regs_read), MP_ROM_PTR(&mp_target_regs_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_regs_write), MP_ROM_PTR(&mp_target_regs_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_reg_read), MP_ROM_PTR(&mp_target_reg_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_reg_write), MP_ROM_PTR(&mp_target_reg_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&mp_target_reset_obj)},
    {MP_ROM_QSTR(MP_QSTR_halt_request),
     MP_ROM_PTR(&mp_target_halt_request_obj)},
    {MP_ROM_QSTR(MP_QSTR_halt_poll), MP_ROM_PTR(&mp_target_halt_poll_obj)},
    {MP_ROM_QSTR(MP_QSTR_halt_resume), MP_ROM_PTR(&mp_target_halt_resume_obj)},
    {MP_ROM_QSTR(MP_QSTR_cmdline), MP_ROM_PTR(&mp_target_cmdline_obj)},
    {MP_ROM_QSTR(MP_QSTR_heapsize), MP_ROM_PTR(&mp_target_heapsize_obj)},
    {MP_ROM_QSTR(MP_QSTR_breakwatch_set),
     MP_ROM_PTR(&mp_target_breakwatch_set_obj)},
    {MP_ROM_QSTR(MP_QSTR_breakwatch_clear),
     MP_ROM_PTR(&mp_target_breakwatch_clear_obj)},

    {MP_ROM_QSTR(MP_QSTR_HALT_RUNNING), MP_ROM_INT(TARGET_HALT_RUNNING)},
    {MP_ROM_QSTR(MP_QSTR_HALT_ERROR), MP_ROM_INT(TARGET_HALT_ERROR)},
    {MP_ROM_QSTR(MP_QSTR_HALT_REQUEST), MP_ROM_INT(TARGET_HALT_REQUEST)},
    {MP_ROM_QSTR(MP_QSTR_HALT_STEPPING), MP_ROM_INT(TARGET_HALT_STEPPING)},
    {MP_ROM_QSTR(MP_QSTR_HALT_BREAKPOINT), MP_ROM_INT(TARGET_HALT_BREAKPOINT)},
    {MP_ROM_QSTR(MP_QSTR_HALT_WATCHPOINT), MP_ROM_INT(TARGET_HALT_WATCHPOINT)},
    {MP_ROM_QSTR(MP_QSTR_HALT_FAULT), MP_ROM_INT(TARGET_HALT_FAULT)},
    {MP_ROM_QSTR(MP_QSTR_BREAK_SOFT), MP_ROM_INT(TARGET_BREAK_SOFT)},
    {MP_ROM_QSTR(MP_QSTR_BREAK_HARD), MP_ROM_INT(TARGET_BREAK_HARD)},
    {MP_ROM_QSTR(MP_QSTR_WATCH_WRITE), MP_ROM_INT(TARGET_WATCH_WRITE)},
    {MP_ROM_QSTR(MP_QSTR_WATCH_READ), MP_ROM_INT(TARGET_WATCH_READ)},
    {MP_ROM_QSTR(MP_QSTR_WATCH_ACCESS), MP_ROM_INT(TARGET_WATCH_ACCESS)},
};

MP_DEFINE_CONST_DICT(target_globals_dict, target_module_globals_table);

// target object
typedef struct _mp_obj_target_t {
  mp_obj_base_t base;
} mp_obj_target_t;

// target module object
const mp_obj_module_t target_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&target_globals_dict,
};

// Register the module to make it available in Python
MP_REGISTER_MODULE(MP_QSTR_target, target_module, (MODULE_BMP_ENABLED));

/* code */

/* translate a black magic probe exception into a micropython exception */

static void raise_mp_exception(struct exception e) {
  switch (e.type) {
  case EXCEPTION_TIMEOUT:
    mp_raise_RuntimeError(MP_ERROR_TEXT("Timeout. Is target stuck in WFI?"));
    break;
  case EXCEPTION_ERROR:
    mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("Exception: %s"),
                      e.msg);
    break;
  }
  return;
}

/* access to bytes/bytearray contents and length */

static void mp_obj_get_data(mp_obj_t o, size_t *len, mp_obj_t **items) {
  if (mp_obj_is_type(MP_OBJ_FROM_PTR(o), &mp_type_bytearray)) {
    mp_obj_array_t *barray = MP_OBJ_FROM_PTR(o);
    *len = barray->len;
    *items = barray->items;
    return;
  }
  if (mp_obj_is_type(MP_OBJ_FROM_PTR(o), &mp_type_bytes) || mp_obj_is_type(MP_OBJ_FROM_PTR(o), &mp_type_str)) {
    mp_obj_str_t *str = MP_OBJ_FROM_PTR(o);
    *len = str->len;
    *items = (void *)str->data;
    return;
  }
  mp_raise_TypeError(MP_ERROR_TEXT("object not string, bytes nor bytearray"));
}

STATIC mp_obj_t mp_target_help() {
  mp_print_str(MP_PYTHON_PRINTER, target_help_text);
  return mp_const_none;
}

STATIC mp_obj_t mp_target_list_targets();

/* jtag scan */

STATIC mp_obj_t mp_target_jtag_scan(mp_obj_t assert_srst_obj,
                                    mp_obj_t ir_len_obj) {
  bool assert_srst_pin = mp_obj_is_true(assert_srst_obj);

  /* ir_len_obj is list of ints (instruction register lengths
    of the devices on the jtag chain). if list is empty, just probe */
  uint8_t irlens[JTAG_MAX_DEVS + 1] = {0};
  if (!mp_obj_is_type(MP_OBJ_FROM_PTR(ir_len_obj), &mp_type_list))
    mp_raise_ValueError(MP_ERROR_TEXT("expected irlen list"));
  mp_obj_list_t *ir_len = MP_OBJ_TO_PTR(ir_len_obj);
  if (ir_len->len > JTAG_MAX_DEVS)
    mp_raise_ValueError(MP_ERROR_TEXT("irlen > JTAG_MAX_DEVS"));
  for (unsigned i = 0; i < ir_len->len; i++) {
    mp_int_t new_irlen = mp_obj_get_int(ir_len->items[i]);
    if (new_irlen > JTAG_MAX_IR_LEN)
      mp_raise_ValueError(MP_ERROR_TEXT("irlen > JTAG_MAX_IR_LEN"));
    irlens[i] = new_irlen;
  }

  if (assert_srst_pin)
    platform_srst_set_val(true); /* will be deasserted after attach */

  int devs = -1;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      devs = jtag_scan(ir_len->len > 0 ? irlens : NULL);
    }
    raise_mp_exception(e);
  }

  if (devs <= 0) {
    platform_srst_set_val(false);
    mp_raise_RuntimeError(MP_ERROR_TEXT("jtag device scan failed"));
    return mp_const_none;
  }
  return mp_target_list_targets();
}

STATIC mp_obj_t mp_target_swdp_scan(mp_obj_t assert_srst_obj) {
  bool assert_srst_pin = mp_obj_is_true(assert_srst_obj);
  int devs = -1;

  if (assert_srst_pin)
    platform_srst_set_val(true);

  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) { devs = adiv5_swdp_scan(); }
    raise_mp_exception(e);
  }

  if (devs <= 0) {
    platform_srst_set_val(false);
    mp_raise_RuntimeError(MP_ERROR_TEXT("sw-dp scan failed"));
    return mp_const_none;
  }

  return mp_target_list_targets();
}

STATIC mp_obj_t mp_target_list_targets() {
  mp_obj_t res = mp_obj_new_list(0, NULL);
  for (target *t = target_list; t != NULL; t = t->next) {
    if (target_core_name(t)) {
      char *label = malloc(strlen(target_driver_name(t)) +
                           strlen(target_core_name(t)) + 2);
      strcpy(label, target_driver_name(t));
      strcat(label, " ");
      strcat(label, target_core_name(t));
      mp_obj_list_append(res, mp_obj_new_str(label, strlen(label)));
      free(label);
    } else
      mp_obj_list_append(res, mp_obj_new_str(target_driver_name(t),
                                             strlen(target_driver_name(t))));
  }
  return res;
}

STATIC mp_obj_t mp_target_attach(mp_obj_t n_obj) {
  mp_int_t n = mp_obj_get_int(n_obj);

  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      cur_target = target_attach_n(n, &gdb_controller);
    }
    raise_mp_exception(e);
  }

  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  return mp_const_none;
}

STATIC mp_obj_t mp_target_detach() {
  if (!cur_target)
    return mp_const_none;

  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) { target_detach(cur_target); }
    raise_mp_exception(e);
  }

  last_target = cur_target;
  cur_target = NULL;
  return mp_const_none;
}

/* re-attach if current target disconnected */
STATIC mp_obj_t mp_target_reattach() {
  if (!cur_target && last_target) {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      cur_target = target_attach(last_target, &gdb_controller);
    }
    raise_mp_exception(e);
  }
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  return mp_const_none;
}

STATIC mp_obj_t mp_target_attached() {
  if (cur_target && cur_target->attached)
    return mp_const_true;
  return mp_const_false;
}

STATIC mp_obj_t mp_target_driver_name() {
  if (cur_target && cur_target->driver)
    return mp_obj_new_str(cur_target->driver, strlen(cur_target->driver));
  return mp_const_none;
}

STATIC mp_obj_t mp_target_core_name() {
  if (cur_target && cur_target->core)
    return mp_obj_new_str(cur_target->core, strlen(cur_target->core));
  return mp_const_none;
}

STATIC mp_obj_t mp_target_check_error() {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  bool ret_code = false;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) { ret_code = target_check_error(cur_target); }
    raise_mp_exception(e);
  }
  return mp_obj_new_bool(ret_code);
}

STATIC mp_obj_t mp_target_mem_map() {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));

  mp_obj_t map = mp_obj_new_list(0, NULL);

  /* Map each defined RAM */
  for (struct target_ram *r = cur_target->ram; r; r = r->next) {
    mp_obj_t region = mp_obj_new_dict(0);
    mp_obj_dict_store(region, MP_ROM_QSTR(MP_QSTR_memory_type),
                      MP_ROM_QSTR(MP_QSTR_ram));
    mp_obj_dict_store(region, MP_ROM_QSTR(MP_QSTR_start),
                      mp_obj_new_int(r->start));
    mp_obj_dict_store(region, MP_ROM_QSTR(MP_QSTR_length),
                      mp_obj_new_int(r->length));
    mp_obj_list_append(map, region);
  }

  /* Map each defined Flash */
  for (struct target_flash *f = cur_target->flash; f; f = f->next) {
    mp_obj_t region = mp_obj_new_dict(0);
    mp_obj_dict_store(region, MP_ROM_QSTR(MP_QSTR_memory_type),
                      MP_ROM_QSTR(MP_QSTR_flash));
    mp_obj_dict_store(region, MP_ROM_QSTR(MP_QSTR_start),
                      mp_obj_new_int(f->start));
    mp_obj_dict_store(region, MP_ROM_QSTR(MP_QSTR_length),
                      mp_obj_new_int(f->length));
    mp_obj_dict_store(region, MP_ROM_QSTR(MP_QSTR_blocksize),
                      mp_obj_new_int(f->blocksize));
    mp_obj_list_append(map, region);
  }

  return map;
}

STATIC mp_obj_t mp_target_mem_map_xml() {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  char buf[2048];
  if (target_mem_map(cur_target, buf, sizeof(buf)))
    return mp_obj_new_str(buf, strlen(buf));
  else {
    mp_raise_RuntimeError(MP_ERROR_TEXT("increase buf"));
    return mp_const_none;
  }
}

STATIC mp_obj_t mp_target_mem_read(mp_obj_t dst_obj, mp_obj_t src_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  target_addr src = mp_obj_get_int(src_obj);
  size_t len;
  mp_obj_t *items;
  mp_obj_get_data(dst_obj, &len, &items);
  if (len == 0)
    return mp_const_none;
  int ret_code = 0;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      ret_code = target_mem_read(cur_target, items, src, len);
    }
    raise_mp_exception(e);
  }
  return mp_obj_new_int(ret_code);
}

STATIC mp_obj_t mp_target_mem_write(mp_obj_t dst_obj, mp_obj_t src_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  target_addr dst = mp_obj_get_int(dst_obj);
  size_t len;
  mp_obj_t *items;
  mp_obj_get_data(src_obj, &len, &items);
  if (len == 0)
    return mp_const_none;
  int ret_code = 0;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      ret_code = target_mem_write(cur_target, dst, items, len);
    }
    raise_mp_exception(e);
  }
  return mp_obj_new_int(ret_code);
}

STATIC mp_obj_t mp_target_flash_erase(mp_obj_t addr_obj, mp_obj_t len_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  target_addr addr = mp_obj_get_int(addr_obj);
  size_t len = mp_obj_get_int(addr_obj);
  if (len == 0)
    return mp_const_none;
  int ret_code = 0;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      ret_code = target_flash_erase(cur_target, addr, len);
    }
    raise_mp_exception(e);
  }
  return mp_obj_new_int(ret_code);
}

STATIC mp_obj_t mp_target_flash_write(mp_obj_t dst_addr_obj, mp_obj_t src_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  target_addr dst_addr = mp_obj_get_int(dst_addr_obj);
  size_t len;
  mp_obj_t *items;
  mp_obj_get_data(src_obj, &len, &items);
  if (len == 0)
    return mp_const_none;
  int ret_code = 0;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      ret_code = target_flash_write(cur_target, dst_addr, items, len);
    }
    raise_mp_exception(e);
  }
  return mp_obj_new_int(ret_code);
}

STATIC mp_obj_t mp_target_flash_done() {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  int ret_code = 0;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) { ret_code = target_flash_done(cur_target); }

    raise_mp_exception(e);
  }
  return mp_obj_new_int(ret_code);
}

STATIC mp_obj_t mp_target_regs_size() {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  return mp_obj_new_int(cur_target->regs_size);
}

STATIC mp_obj_t mp_target_tdesc_xml() {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  if (cur_target->tdesc)
    return mp_obj_new_str(cur_target->tdesc, strlen(cur_target->tdesc));
  return mp_const_none;
}

STATIC mp_obj_t mp_target_regs_read(mp_obj_t data_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  size_t len;
  mp_obj_t *items;
  mp_obj_get_data(data_obj, &len, &items);
  if (len == cur_target->regs_size) {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) { target_regs_read(cur_target, items); }
    raise_mp_exception(e);
  }
  return mp_const_none;
}

STATIC mp_obj_t mp_target_regs_write(mp_obj_t data_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  size_t len;
  mp_obj_t *items;
  mp_obj_get_data(data_obj, &len, &items);
  if (len == cur_target->regs_size) {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) { target_regs_write(cur_target, items); }
    raise_mp_exception(e);
  }
  return mp_const_none;
}

STATIC mp_obj_t mp_target_reg_read(mp_obj_t reg_obj, mp_obj_t data_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  int reg = mp_obj_get_int(reg_obj);
  size_t len;
  mp_obj_t *items;
  mp_obj_get_data(data_obj, &len, &items);
  int ret_code = 0;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      ret_code = target_reg_read(cur_target, reg, items, len);
    }
    raise_mp_exception(e);
  }
  return mp_obj_new_int(ret_code);
}

STATIC mp_obj_t mp_target_reg_write(mp_obj_t reg_obj, mp_obj_t data_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  int reg = mp_obj_get_int(reg_obj);
  size_t len;
  mp_obj_t *items;
  mp_obj_get_data(data_obj, &len, &items);
  int ret_code = 0;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      ret_code = target_reg_write(cur_target, reg, items, len);
    }
    raise_mp_exception(e);
  }
  return mp_obj_new_int(ret_code);
}

STATIC mp_obj_t mp_target_reset() {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) { target_reset(cur_target); }
    raise_mp_exception(e);
  }
  return mp_const_none;
}

STATIC mp_obj_t mp_target_halt_request() {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) { target_halt_request(cur_target); }
    raise_mp_exception(e);
  }
  return mp_const_none;
}

/* check whether processor running or halted.
 * if the processor is halted at a watchpoint,
 * also return the address of the watched variable */

STATIC mp_obj_t mp_target_halt_poll() {
  qstr reason[] = {MP_QSTR_RUNNING,  MP_QSTR_ERROR,      MP_QSTR_REQUEST,
                   MP_QSTR_STEPPING, MP_QSTR_BREAKPOINT, MP_QSTR_WATCHPOINT,
                   MP_QSTR_FAULT};
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));

  /* get status */
  target_addr watchpt_addr = 0;
  int ret_code = 0;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      ret_code = target_halt_poll(cur_target, &watchpt_addr);
    }
    raise_mp_exception(e);
  }

  /* return tuple of result code, reason, and watchpoint address */
  int reason_qstr;
  if ((ret_code >= TARGET_HALT_RUNNING) && (ret_code <= TARGET_HALT_FAULT))
    reason_qstr = reason[ret_code];
  else
    reason_qstr = MP_QSTR_ERROR;
  mp_obj_t res_obj = mp_obj_new_tuple(3, NULL);
  mp_obj_tuple_t *res = MP_OBJ_TO_PTR(res_obj);
  res->items[0] = mp_obj_new_int(ret_code);
  res->items[1] = MP_ROM_QSTR(reason_qstr);
  res->items[2] = mp_obj_new_int(watchpt_addr);
  return res_obj;
}

STATIC mp_obj_t mp_target_halt_resume(mp_obj_t step_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  bool step = mp_obj_is_true(step_obj);
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) { target_halt_resume(cur_target, step); }
    raise_mp_exception(e);
  }
  return mp_const_none;
}

STATIC mp_obj_t mp_target_cmdline(mp_obj_t cmdline_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  char *cmdline = (char *)mp_obj_str_get_str(cmdline_obj);
  target_set_cmdline(cur_target, cmdline);
  return mp_const_none;
}

STATIC mp_obj_t mp_target_heapsize(mp_obj_t heapsize_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  mp_obj_list_t *heapsize = MP_OBJ_TO_PTR(heapsize_obj);
  if (heapsize->len != 4)
    mp_raise_ValueError(MP_ERROR_TEXT("heapsize len != 4"));
  target_addr heap_base = mp_obj_get_int(heapsize->items[0]);
  target_addr heap_limit = mp_obj_get_int(heapsize->items[1]);
  target_addr stack_base = mp_obj_get_int(heapsize->items[2]);
  target_addr stack_limit = mp_obj_get_int(heapsize->items[3]);
  target_set_heapinfo(cur_target, heap_base, heap_limit, stack_base,
                      stack_limit);
  return mp_const_none;
}

STATIC mp_obj_t mp_target_breakwatch_set(mp_obj_t bw_type_obj,
                                         mp_obj_t addr_obj, mp_obj_t len_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  mp_int_t bw_type = mp_obj_get_int(bw_type_obj);
  target_addr addr = mp_obj_get_int(addr_obj);
  size_t len = mp_obj_get_int(len_obj);
  int ret_code = 0;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      ret_code = target_breakwatch_set(cur_target, bw_type, addr, len);
    }
    raise_mp_exception(e);
  }
  return mp_obj_new_int(ret_code);
}

STATIC mp_obj_t mp_target_breakwatch_clear(mp_obj_t bw_type_obj,
                                           mp_obj_t addr_obj,
                                           mp_obj_t len_obj) {
  if (!cur_target)
    mp_raise_RuntimeError(MP_ERROR_TEXT("no target"));
  mp_int_t bw_type = mp_obj_get_int(bw_type_obj);
  target_addr addr = mp_obj_get_int(addr_obj);
  size_t len = mp_obj_get_int(len_obj);
  int ret_code = 0;
  {
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      ret_code = target_breakwatch_clear(cur_target, bw_type, addr, len);
    }
    raise_mp_exception(e);
  }
  return mp_obj_new_int(ret_code);
}
