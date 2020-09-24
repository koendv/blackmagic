#include "general.h"
#include "exception.h"
#include "adiv5.h"
#include "target.h"
#include "target_internal.h"
#include "cortexm.h"
#include "platform.h"
#include "command.h"
#include "mod_bmp.h"

#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mpthread.h"
#include "py/mperrno.h"
#include "py/mpprint.h"
#include "py/mphal.h"
#include "py/objmodule.h"
#include "py/objarray.h"
#include "py/objstr.h"

static const char fun_err[] = "semihosting function not found\n";
static const char mod_err[] = "no module named 'semihosting'\n";

/*
 * Semihosting
 * Semihosting i/o is to micropython console;
 * and semihosting file i/o is to micropython filesystem (sdcard).
 *
 * Many of the semihosting calls are implemented in the
 * micropython script 'semihosting.py'. This script needs to be
 * imported, and semihosting.init() called, for the semihosting calls to run.
 *
 * You can speed up a semihosting call by writing it in c;
 * or make the implementation flexible by writing it in micropython.
 */

/* ARM Semihosting syscall numbers,
 * from "Semihosting for AArch32 and AArch64 Version 3.0" */

#define SYS_CLOCK 0x10
#define SYS_CLOSE 0x02
#define SYS_ELAPSED 0x30
#define SYS_ERRNO 0x13
#define SYS_EXIT 0x18
#define SYS_EXIT_EXTENDED 0x20
#define SYS_FLEN 0x0C
#define SYS_GET_CMDLINE 0x15
#define SYS_HEAPINFO 0x16
#define SYS_ISERROR 0x08
#define SYS_ISTTY 0x09
#define SYS_OPEN 0x01
#define SYS_READ 0x06
#define SYS_READC 0x07
#define SYS_REMOVE 0x0E
#define SYS_RENAME 0x0F
#define SYS_SEEK 0x0A
#define SYS_SYSTEM 0x12
#define SYS_TICKFREQ 0x31
#define SYS_TIME 0x11
#define SYS_TMPNAM 0x0D
#define SYS_WRITE 0x05
#define SYS_WRITEC 0x03
#define SYS_WRITE0 0x04

#define TARGET_NULL ((target_addr)0)

int mp_semihosting_call_0(qstr fn_name);
int mp_semihosting_call_1(qstr fn_name, mp_obj_t fn_arg0);
int mp_semihosting_call_2(qstr fn_name, mp_obj_t fn_arg0, mp_obj_t fn_arg1);
int mp_semihosting_call_3(qstr fn_name, mp_obj_t fn_arg0, mp_obj_t fn_arg1,
                          mp_obj_t fn_arg2);
mp_obj_t mp_semihosting_call_2_obj(qstr fn_name, mp_obj_t fn_arg0,
                                   mp_obj_t fn_arg1);
mp_obj_t mp_target_str_obj(target *t, target_addr taddr, size_t len);

static uint32_t time0_millis = UINT32_MAX; /* sys_clock time origin */

int cortexm_hostio_request(target *t) {
  uint32_t arm_regs[t->regs_size];
  uint32_t params[4];

  bmp_enabled = false;
  t->tc->interrupted = false;
  target_regs_read(t, arm_regs);
  target_mem_read(t, params, arm_regs[1], sizeof(params));
  uint32_t syscall = arm_regs[0];
  int32_t ret = 0;

  DEBUG_INFO("syscall 0" PRIx32 "%" PRIx32 " (%" PRIx32 " %" PRIx32 " %" PRIx32
             " %" PRIx32 ")\n",
             syscall, params[0], params[1], params[2], params[3]);
  switch (syscall) {

  case SYS_CLOCK: { /* clock */
    uint32_t curr_millis = mp_hal_ticks_ms();
    if (time0_millis > curr_millis)
      time0_millis = curr_millis;
    curr_millis -= time0_millis;
    ret = curr_millis / 10;
    break;
  }

  case SYS_CLOSE: { /* close */
    mp_obj_t fh_obj = mp_obj_new_int(params[0]);
    ret = mp_semihosting_call_1(MP_QSTR_close, fh_obj);
    break;
  }

  case SYS_ELAPSED: { /* elapsed */
    ret = mp_semihosting_call_0(MP_QSTR_elapsed);
    break;
  }

  case SYS_ERRNO: { /* errno */
    ret = mp_semihosting_call_0(MP_QSTR_errno);
    break;
  }

  case SYS_EXIT: { /* _exit() */
    mp_printf(&mp_plat_print, "_exit(0x%x)\n", params[0]);
    t->tc->interrupted = true;
    break;
  }

  case SYS_EXIT_EXTENDED: { /* _exit() */
    mp_printf(&mp_plat_print, "_exit(0x%x%08x)\n", params[1], params[0]);
    t->tc->interrupted = true;
    break;
  }

  case SYS_FLEN: { /* stat */
    mp_obj_t fh_obj = mp_obj_new_int(params[0]);
    ret = mp_semihosting_call_1(MP_QSTR_flen, fh_obj);
    break;
  }

  case SYS_GET_CMDLINE: { /* get_cmdline */
    uint32_t retval[2];
    ret = -1;
    target_addr buf_ptr = params[0];
    target_addr buf_len = params[1];
    if (strlen(t->cmdline) + 1 > buf_len)
      break;
    if (target_mem_write(t, buf_ptr, t->cmdline, strlen(t->cmdline) + 1))
      break;
    retval[0] = buf_ptr;
    retval[1] = strlen(t->cmdline) + 1;
    if (target_mem_write(t, arm_regs[1], retval, sizeof(retval)))
      break;
    ret = 0;
    break;
  }

  case SYS_HEAPINFO: { /* heapinfo */
    target_mem_write(t, arm_regs[1], &t->heapinfo,
                     sizeof(t->heapinfo)); /* See newlib/libc/sys/arm/crt0.S */
    break;
  }

  case SYS_ISERROR: { /* iserror */
    mp_obj_t errno_obj = mp_obj_new_int(params[0]);
    ret = mp_semihosting_call_1(MP_QSTR_iserror, errno_obj);
    break;
  }

  case SYS_ISTTY: { /* istty */
    mp_obj_t fh_obj = mp_obj_new_int(params[0]);
    ret = mp_semihosting_call_1(MP_QSTR_istty, fh_obj);
    break;
  }

  case SYS_OPEN: { /* open */
    target_addr fnam_taddr = params[0];
    uint32_t fnam_len = params[2];
    uint32_t flags = params[1];
    mp_obj_t fnam_obj = mp_target_str_obj(t, fnam_taddr, fnam_len);
    mp_obj_t flags_obj = mp_obj_new_int(flags);
    ret = mp_semihosting_call_2(MP_QSTR_open, fnam_obj, flags_obj);
    break;
  }

  case SYS_READ: { /* read */
    ret = -1;
    /* get syscall arguments */
    mp_obj_t fh_obj = mp_obj_new_int(params[0]); // file handle
    target_addr buf_taddr = params[1];           // buffer address
    uint32_t buf_len = params[2];                // buffer length
    ret = buf_len;
    mp_obj_t buf_len_obj = mp_obj_new_int(params[2]);
    /* call python; returns string, bytes or bytearray */
    mp_obj_t ret_obj =
        mp_semihosting_call_2_obj(MP_QSTR_read, fh_obj, buf_len_obj);
    /* extract data from return object */
    size_t len = 0;
    mp_obj_t *dta = NULL;
    if (mp_obj_is_type(MP_OBJ_FROM_PTR(ret_obj), &mp_type_bytearray)) {
      mp_obj_array_t *barray = MP_OBJ_FROM_PTR(ret_obj);
      len = barray->len;
      dta = barray->items;
    } else if (mp_obj_is_type(MP_OBJ_FROM_PTR(ret_obj), &mp_type_bytes) ||
               mp_obj_is_type(MP_OBJ_FROM_PTR(ret_obj), &mp_type_str)) {
      mp_obj_str_t *str = MP_OBJ_FROM_PTR(ret_obj);
      len = str->len;
      dta = (void *)str->data;
    }
    if (len == 0)
      break;
    if (len > buf_len)
      len = buf_len;
    /* store  data in target memory */
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) { target_mem_write(t, buf_taddr, dta, len); }
    if (e.type || target_check_error(t))
      break;
    ret = buf_len - len;
    break;
  }

  case SYS_READC: { /* getchar */
    ret = mp_semihosting_call_0(MP_QSTR_readc);
    break;
  }

  case SYS_REMOVE: { /* remove */
    target_addr fnam1_taddr = params[0];
    uint32_t fnam1_len = params[1];
    mp_obj_t fnam1_obj = mp_target_str_obj(t, fnam1_taddr, fnam1_len);
    ret = mp_semihosting_call_1(MP_QSTR_remove, fnam1_obj);
    break;
  }

  case SYS_RENAME: { /* rename */
    target_addr fnam1_taddr = params[0];
    uint32_t fnam1_len = params[1];
    mp_obj_t fnam1_obj = mp_target_str_obj(t, fnam1_taddr, fnam1_len);
    target_addr fnam2_taddr = params[2];
    uint32_t fnam2_len = params[3];
    mp_obj_t fnam2_obj = mp_target_str_obj(t, fnam2_taddr, fnam2_len);
    ret = mp_semihosting_call_2(MP_QSTR_rename, fnam1_obj, fnam2_obj);
    break;
  }

  case SYS_SEEK: { /* seek */
    mp_obj_t fh_obj = mp_obj_new_int(params[0]);
    mp_obj_t offset_obj = mp_obj_new_int(params[1]);
    ret = mp_semihosting_call_2(MP_QSTR_seek, fh_obj, offset_obj);
    break;
  }

  case SYS_SYSTEM: { /* system */
    target_addr cmd_taddr = params[0];
    uint32_t cmd_len = params[1];
    mp_obj_t cmd_obj = mp_target_str_obj(t, cmd_taddr, cmd_len);
    ret = mp_semihosting_call_1(MP_QSTR_system, cmd_obj);
    break;
  }

  case SYS_TICKFREQ: { /* tickfreq */
    ret = mp_semihosting_call_0(MP_QSTR_tickfreq);
    break;
  }

  case SYS_TIME: { /* time */
    ret = unix_time_ms();
    break;
  }

  case SYS_TMPNAM: { /* tmpnam */
    /* Given a target identifier between 0 and 255, returns a temporary name */
    target_addr buf_ptr = params[0];
    int target_id = params[1];
    int buf_size = params[2];
    char fnam[] = "tempXX.tmp";
    ret = -1;
    if (buf_ptr == 0)
      break;
    if (buf_size <= 0)
      break;
    if ((target_id < 0) || (target_id > 255))
      break;                           /* target id out of range */
    fnam[5] = 'A' + (target_id & 0xF); /* create filename */
    fnam[4] = 'A' + (target_id >> 4 & 0xF);
    if (strlen(fnam) + 1 > (uint32_t)buf_size)
      break; /* target buffer too small */
    if (target_mem_write(t, buf_ptr, fnam, strlen(fnam) + 1))
      break; /* copy filename to target */
    ret = 0;
    break;
  }

  case SYS_WRITE: { /* write */
    mp_obj_t fh_obj = mp_obj_new_int(params[0]);
    target_addr target_buf = params[1];
    uint32_t buf_len = params[2];
    ret = buf_len;
    byte buf[buf_len];
    volatile struct exception e;
    TRY_CATCH(e, EXCEPTION_ALL) {
      target_mem_read(t, buf, target_buf, buf_len);
    }
    if (e.type || target_check_error(t))
      break;
    mp_obj_t buf_obj = mp_obj_new_bytes(buf, buf_len);
    ret = mp_semihosting_call_2(MP_QSTR_write, fh_obj, buf_obj);
    break;
  }

  case SYS_WRITEC: { /* writec */
    ret = -1;
    uint8_t ch;
    target_addr ch_taddr = arm_regs[1];
    if (ch_taddr == TARGET_NULL)
      break;
    ch = target_mem_read8(t, ch_taddr);
    if (target_check_error(t))
      break;
    mp_print_strn(&mp_plat_print, (const char *)&ch, 1, 0, 0, 0);
    ret = 0;
    break;
  }

  case SYS_WRITE0: { /* write0 */
    ret = -1;
    uint8_t ch;
    uint8_t buf[256];
    size_t len;
    target_addr str = arm_regs[1];
    len = 0;
    if (str == TARGET_NULL)
      break;
    while ((ch = target_mem_read8(t, str++)) != '\0') {
      if (target_check_error(t))
        break;
      buf[len++] = ch;
      if (len == sizeof(buf)) {
        mp_print_strn(&mp_plat_print, (const char *)buf, len, 0, 0, 0);
        len = 0;
      }
    }
    if (len != 0)
      mp_print_strn(&mp_plat_print, (const char *)buf, len, 0, 0, 0);
    ret = 0;
    break;
  }

  default:
    ret = -1;
    break;
  }

  arm_regs[0] = ret;
  target_regs_write(t, arm_regs);
  bmp_enabled = true;

  return t->tc->interrupted;
}

/* semihosting call: no arguments, returns int */

int mp_semihosting_call_0(qstr fn_name) {
  int ret = -1;
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_obj_t sh_module_obj = mp_module_get(MP_QSTR_semihosting);
    if (sh_module_obj) {
      mp_obj_t sh_fn = mp_load_attr(sh_module_obj, fn_name);
      if (sh_fn) {
        mp_obj_t ret_obj = mp_call_function_0(sh_fn);
        if ((ret_obj != MP_OBJ_NULL) && mp_obj_is_int(ret_obj))
          ret = mp_obj_get_int(ret_obj);
      } else
        mp_print_str(&mp_plat_print, fun_err);
    } else
      mp_print_str(&mp_plat_print, mod_err);
    nlr_pop();
  } else
    mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
  return ret;
}

/* semihosting call: 1 argument, returns int */

int mp_semihosting_call_1(qstr fn_name, mp_obj_t fn_arg0) {
  int ret = -1;
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_obj_t sh_module_obj = mp_module_get(MP_QSTR_semihosting);
    if (sh_module_obj) {
      mp_obj_t sh_fn = mp_load_attr(sh_module_obj, fn_name);
      if (sh_fn) {
        mp_obj_t ret_obj = mp_call_function_1(sh_fn, fn_arg0);
        if ((ret_obj != MP_OBJ_NULL) && mp_obj_is_int(ret_obj))
          ret = mp_obj_get_int(ret_obj);
      } else
        mp_print_str(&mp_plat_print, fun_err);
    } else
      mp_print_str(&mp_plat_print, mod_err);
    nlr_pop();
  } else
    mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
  return ret;
}

/* semihosting call: 2 arguments, returns int */

int mp_semihosting_call_2(qstr fn_name, mp_obj_t fn_arg0, mp_obj_t fn_arg1) {
  int ret = -1;
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_obj_t sh_module_obj = mp_module_get(MP_QSTR_semihosting);
    if (sh_module_obj) {
      mp_obj_t sh_fn = mp_load_attr(sh_module_obj, fn_name);
      if (sh_fn) {
        mp_obj_t ret_obj = mp_call_function_2(sh_fn, fn_arg0, fn_arg1);
        if ((ret_obj != MP_OBJ_NULL) && mp_obj_is_int(ret_obj))
          ret = mp_obj_get_int(ret_obj);
      } else
        mp_print_str(&mp_plat_print, fun_err);
    } else
      mp_print_str(&mp_plat_print, mod_err);
    nlr_pop();
  } else
    mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
  return ret;
}

/* semihosting call: 2 arguments, returns mp object */

mp_obj_t mp_semihosting_call_2_obj(qstr fn_name, mp_obj_t fn_arg0,
                                   mp_obj_t fn_arg1) {
  mp_obj_t ret_obj = mp_const_none;
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_obj_t sh_module_obj = mp_module_get(MP_QSTR_semihosting);
    if (sh_module_obj) {
      mp_obj_t sh_fn = mp_load_attr(sh_module_obj, fn_name);
      if (sh_fn) {
        ret_obj = mp_call_function_2(sh_fn, fn_arg0, fn_arg1);
      } else
        mp_print_str(&mp_plat_print, fun_err);
    } else
      mp_print_str(&mp_plat_print, mod_err);
    nlr_pop();
  } else
    mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
  return ret_obj;
}

/* convert string in target memory in mp object */

mp_obj_t mp_target_str_obj(target *t, target_addr taddr, size_t len) {
  // if ((len > MAX_LEN)) return mp_obj_none;
  char buf[len + 1];
  target_mem_read(t, buf, taddr, len + 1);
  buf[len + 1] = '\0';
  return mp_obj_new_str(buf, len);
}

// not truncated
