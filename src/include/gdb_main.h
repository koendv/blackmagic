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

#ifndef __GDB_MAIN_H
#define __GDB_MAIN_H

/* process one gdb remote command */
void gdb_main(char *pbuf, int size);

/* global variable to tell bmp_loop() to poll target */
extern bool gdb_target_running;
/* called when user types ctrl-c */
void gdb_halt_target(void);
/* poll running target */
void gdb_poll_target(void);
/* access for mp target */
extern target *cur_target;
extern target *last_target;
extern struct target_controller gdb_controller;

#endif

