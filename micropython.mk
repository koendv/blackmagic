###############################################################################
# bmp

ifeq ($(MODULE_BMP_ENABLED),1)
BMP_DIR := $(USERMOD_DIR)
CFLAGS_MOD += -DNO_LIBOPENCM3 -DMICROPYTHON -DPC_HOSTED=0 -DENABLE_DEBUG -Wno-pointer-arith -Wno-char-subscripts
INC += \
  -I$(BMP_DIR)/src/include \
  -I$(BMP_DIR)/src/target \
  -I$(BMP_DIR)/src/platforms/$(MCU_ARCH) \
  -I$(BMP_DIR)/src/platforms/$(MCU_SERIES)

SRC_MOD += $(addprefix $(BMP_DIR)/src/,\
  mod_bmp.c \
  mod_target.c \
  command.c \
  crc32.c \
  crc32_hw.c \
  exception.c \
  gdb_hostio.c \
  gdb_main.c \
  gdb_packet.c \
  gdb_rx.c \
  gdb_rxtx.c \
  gdb_tx.c \
  hex_utils.c \
  malloc.c \
  morse.c \
  mp_semihosting.c \
  timing.c \
  tracebuf.c \
  traceswo.c \
  platforms/common/jtagtap.c \
  platforms/common/swdptap.c \
  platforms/$(MCU_ARCH)/common.c \
  platforms/$(MCU_SERIES)/platform.c \
  target/adiv5.c \
  target/adiv5_jtagdp.c \
  target/adiv5_swdp.c \
  target/cortexa.c \
  target/cortexm.c \
  target/efm32.c \
  target/jtag_devs.c \
  target/jtag_scan.c \
  target/kinetis.c \
  target/lmi.c \
  target/lpc11xx.c \
  target/lpc15xx.c \
  target/lpc17xx.c \
  target/lpc43xx.c \
  target/lpc_common.c \
  target/msp432.c \
  target/nrf51.c \
  target/nxpke04.c \
  target/sam3x.c \
  target/sam4l.c \
  target/samd.c \
  target/samx5x.c \
  target/stm32f1.c \
  target/stm32f4.c \
  target/stm32h7.c \
  target/stm32l0.c \
  target/stm32l4.c \
  target/target.c \
  )

$(BMP_DIR)/src/command.c: $(BMP_DIR)/src/include/version.h

$(BMP_DIR)/src/include/version.h:
	$(Q)echo "#ifndef FIRMWARE_VERSION\n#define FIRMWARE_VERSION \"$(shell cd $(BMP_DIR); git describe --always --dirty --tags)\"\n#endif" > $@

ifeq ($(BMP_ADD_MISSING),1)
SRC_MOD += $(addprefix $(BMP_DIR)/missing/,\
  abort.c \
  ctype.c \
  errno.c \
  sccl.c \
  ungetc.c \
  impure.c \
  nano-vfscanf.c \
  nano-vfscanf_i.c \
  scanf.c \
  sprintf.c \
  stdlib.c \
  string.c \
  )
endif

endif
#not truncated
