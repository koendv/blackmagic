#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <bmp_malloc.h>

int sprintf(char * __restrict buf, const char * __restrict format, ...)
{
	va_list arg;
	int rv;

	va_start(arg, format);
	rv = vsnprintf(buf, SIZE_MAX/2 /* XXX */, format, arg);
	va_end(arg);

	return rv;
}

int vasprintf(char **__restrict buf, const char * __restrict format,
			 va_list arg)
{

	/* This implementation actually calls the printf machinery twice, but
	 * only does one malloc.  This can be a problem though when custom printf
	 * specs or the %m specifier are involved because the results of the
	 * second call might be different from the first. */
	va_list arg2;
	int rv;

	va_copy(arg2, arg);
 	rv = vsnprintf(NULL, 0, format, arg2);
	va_end(arg2);

	*buf = NULL;

	if (rv >= 0) {
		if ((*buf = malloc(++rv)) != NULL) {
			if ((rv = vsnprintf(*buf, rv, format, arg)) < 0) {
				free(*buf);
				*buf = NULL;
			}
		}
	}

	assert(rv >= -1);

	return rv;
}

