#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int __ssvfscanf_r (struct _reent *rptr, register FILE *fp, char const *fmt0, va_list ap);
extern _READ_WRITE_RETURN_TYPE __seofread (struct _reent *, void *, char *, _READ_WRITE_BUFSIZE_TYPE);

int 
sscanf (const char *__restrict str,
       const char * fmt, ...)
{
  int ret;
  va_list ap;
  FILE f;

  f._flags = __SRD | __SSTR;
  f._bf._base = f._p = (unsigned char *) str;
  f._bf._size = f._r = strlen (str);
  f._read = __seofread;
  f._ub._base = NULL;
  f._lb._base = NULL;
  f._file = -1;  /* No file. */
  va_start (ap, fmt);
  ret = __ssvfscanf_r (_REENT, &f, fmt, ap);
  va_end (ap);
  return ret;
}

/* Dummy function used in sscanf/swscanf. */
_READ_WRITE_RETURN_TYPE
__seofread (struct _reent *_ptr,
       void *cookie,
       char *buf,
       _READ_WRITE_BUFSIZE_TYPE len)
{
  return 0;
}

