# Print output: SDCC's printf() (platforms/mcs51/_print.h), which writes through putchar() (lib/ch555/hal.c).
# lib/printf is not used: SDCC cannot call its output functions through pointers with that many argument bytes.
QUANTUM_SRC += $(QUANTUM_DIR)/logging/print.c
