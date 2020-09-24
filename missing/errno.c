#include <py/mpconfig.h>
#include <errno.h>

int _errno;

int *
__errno ()
{
   return &_errno;
}

