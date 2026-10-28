ifneq (,$(filter $(MCU),ch555))
  PROTOCOL = CH555
  # MCU family and series: the MCU's code is in vendors/$(MCU_FAMILY), its build rules in $(MCU_SERIES).mk there
  MCU_FAMILY = WCH
  MCU_SERIES = CH555

  # Internal RAM: 256 bytes, the upper 128 only through @Ri (SDCC puts the stack and __idata there).
  IRAM_SIZE = 0x0100
  # xRAM is 0x0000-0x1FFF, but the USB buffers are fixed at 0x1000-0x13FF (pUSB_BUF_DEV0 .. pUSB_BUF_HUB in
  # ch555.h), so the linker only gets 0x0100-0x0FFF. Below 0x0100: with the Evision OEM bootloader,
  # each interrupt stub of that bootloader stores 3 bytes in xdata between 0x0042 and 0x00FC. The xSFRs are at
  # 0x2100-0x22FF, not here.
  XRAM_LOC  = 0x0100
  XRAM_SIZE = 0x0f00
  # Code flash up to the data flash (0xF000); a bootloader can lower it (bootloader.mk).
  CODE_SIZE ?= 0xf000

  # Fsys: 12, 16, 24 or 32 MHz; a keyboard's rules.mk may set it
  F_CPU ?= 24000000
endif

ifneq (,$(filter $(MCU),sh68f90a))
  PROTOCOL = SH68F90
  # MCU family and series: the MCU's code is in vendors/$(MCU_FAMILY), its build rules in $(MCU_SERIES).mk there
  MCU_FAMILY = SINOWEALTH
  MCU_SERIES = SH68F90

  # Internal RAM: 256 bytes, the upper 128 only through @Ri.
  IRAM_SIZE = 0x0100
  # xRAM: 0x0000-0x0FFF. The USB buffers are at 0x1100-0x11FF and the PWM registers at 0xFF80-0xFFFF, outside it.
  XRAM_LOC  = 0x0000
  XRAM_SIZE = 0x1000
  # Code flash: 64 KB; a bootloader can lower it (bootloader.mk).
  CODE_SIZE ?= 0x10000

  # SYSCLK: 24 MHz, the only clock with USB
  F_CPU ?= 24000000
endif
