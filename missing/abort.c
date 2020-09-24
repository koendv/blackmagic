#include <py/mpconfig.h>

void abort_(void);
NORETURN void abort() {
  abort_();
	__builtin_unreachable();
}
