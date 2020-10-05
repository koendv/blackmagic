# Black Magic Probe

## Overview

These are sources for a micropython debugger probe. The debugger probe provides

- a gdb server, to debug using gdb 
- a cmsis-dap probe, to debug using openocd and pyocd
- micropython extension modules, to manipulate target memory and registers from micropython scripts.

The sources for the micropython debugger probe are spread over four gits:

- [bmp](http://www.github.com/koendv/bmp): build script, binaries, doc
- [micropython](http://www.github.com/koendv/micropython) micropython, patched. Board definition files.
- [blackmagic](http://www.github.com/koendv/blackmagic) Black Magic Probe, patched into a micropython module
- [free-dap](http://www.github.com/koendv/free-dap) free-dap, patched into a micropython module.

These are the Black Magic Probe source files.

## This git

Black Magic Probe is a gdb debugger probe. This is Black Magic Probe, rewritten as a micropython extension module.

- runs on stm32f7, stm32h7, and esp32 processors

- connection to debugger over wifi (tcp), usb virtual comm port, serial port, micropython stream devices in general.
    - ``bmp.init(tcp=3333)`` for a connection over tcp port 3333
    - ``bmp.init(stream=pyb.USB_VCP(1))`` for a connection over usb virtual comm port
    - ``bmp.init(stream=machine.UART(1, 115200))`` for a connection over serial port 1 at 115200 baud.

- semihosting implementation uses micropython calls, not gdb hostio calls. Semihosting print output and  keyboard input is from the micropython console. Semihosting file i/o is on the micropython file system/sdcard.

- receive swo on any micropython uart. ``bmp.traceswo(uart=machine.UART(1, 2250000)``

- read and write target ram, flash and registers. set and clear breakpoints and watchpoints. ``target.mem_read()``, ``target.mem_write()``.

- bmp porting configuration is split up in architecture, processor and board.

    - architecture specifics in ``extmod/blackmagic/src/platforms/stm32``
    - processor specifics in ``extmod/blackmagic/src/platforms/f7`` 
    - optionally, board-specifics in ``micropython/ports/stm32/boards/*/mpconfigboard.h``, eg. ``PLATFORM_HAS_POWER_SWITCH``.
    - SWD/JTAG pin definitions in micropython ``pins.csv`` file


