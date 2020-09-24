/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2011  Black Sphere Technologies Ltd.
 * Written by Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file implements the GDB Remote Serial Debugging protocol packet
 * reception and transmission as well as some convenience functions.
 */

#include "general.h"
#include "gdb_if.h"
#include "gdb_packet.h"
#include "hex_utils.h"

#include <stdarg.h>

void gdb_putpacket_f(const char *fmt, ...)
{
	va_list ap;
	char *buf;
	int size;

	va_start(ap, fmt);
	size = vasprintf(&buf, fmt, ap);
	gdb_putpacket(buf, size);
	free(buf);
	va_end(ap);
}

void gdb_out(const char *buf)
{
	char *hexdata;
	int i;

	hexdata = alloca((i = strlen(buf)*2 + 1) + 1);
	hexdata[0] = 'O';
	hexify(hexdata+1, buf, strlen(buf));
	gdb_putpacket(hexdata, i);
}

void gdb_voutf(const char *fmt, va_list ap)
{
	char *buf;

	if (vasprintf(&buf, fmt, ap) < 0)
		return;
	gdb_out(buf);
	free(buf);
}

void gdb_outf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	gdb_voutf(fmt, ap);
	va_end(ap);
}
