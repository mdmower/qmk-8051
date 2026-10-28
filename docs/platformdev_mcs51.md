# MCS-51 (8051)

MCS-51 is the platform for keyboards with an 8051 microcontroller. It is compiled with SDCC, which differs from GCC in ways that affect how code is written and built. This page describes the platform for developers: how the build works, what to keep in mind when writing code for SDCC, how the platform code is organized, and what a port to another 8051 MCU involves.

SDCC 4.6.0 is required.

## Supported MCUs

| `processor` | `MCU_FAMILY` | `MCU_SERIES` | USB protocol | Bootloaders              |
| ----------- | ------------ | ------------ | ------------ | ------------------------ |
| `ch555`     | `WCH`        | `CH555`      | `CH555`      | `evision-oem`, `ch55xbl` |

The MCU and bootloader are set in the keyboard's `keyboard.json`. For example, Evision's VS11K28A and VS11K33A are CH555-family parts and both have processor `ch555`:

```json
"processor": "ch555",
"bootloader": "evision-oem",
```

| Bootloader    | Description                                                                                                                                                                 |
| ------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `evision-oem` | Evision OEM bootloader. The image starts at 0x2800, and `qmk compile` writes it to `<target>.bin`. The [root readme](../readme.md#flash-layout) describes the flash layout. |
| `ch55xbl`     | WCH factory bootloader in ROM. The image starts at 0x0000. Untested.                                                                                                        |
| `custom`      | `bootloader_jump()` is defined in the keyboard's code.                                                                                                                      |

## The build

### Compiler driver

QMK's make rules call the compiler with GCC's options. On this platform `$(CC)` is [sdcc_cc.py](../platforms/mcs51/sdcc_cc.py), a driver that accepts those options and runs SDCC so that QMK's compile and link rules work unchanged. The driver also runs three build steps that SDCC does not have:

- **Weak symbol resolution.** SDCC has no weak linkage, and QMK relies on it for its many overridable functions. The driver compiles each preprocessed file with clang as well, lists the file's weak definitions, and at link time keeps the strong definition of each symbol and removes the weak ones. A file that contains weak definitions is compiled with `--nopurity`, because otherwise SDCC removes calls to an empty function defined in the same file, and a keymap's override would never be called. Files with SDCC inline assembly (`__asm` ... `__endasm`) cannot be read by clang; all their definitions count as strong.
- **Checks for miscompiled code.** SDCC 4.6.0 compiles some valid C to incorrect code without a warning. Other SDCC versions may not have the same bugs, which is one reason SDCC 4.6.0 is required. The driver runs the matchers in [sdcc_check.query](../platforms/mcs51/sdcc_check.query) on every file with clang-query and stops the build when one matches. The constructs are listed in [Writing code for SDCC](#writing-code-for-sdcc).
- **Removal of unused functions.** SDCC links every function of every object file, including the copies of `static inline` header functions that SDCC emits into each file that includes the header. At link time the driver removes functions that no reachable code or data refers to, similar to GCC's `--gc-sections`. The list of removed functions is written to `.build/<target>.dce.txt`.

SDCC's linker writes `<target>.ihx`, which the driver converts to ELF so that QMK's `.hex` and `.bin` rules work.

### Build options

The following variables can be set in a keyboard's or keymap's `rules.mk`:

| Variable                       | Default    | Description                                                                                                                                                                            |
| ------------------------------ | ---------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `F_CPU`                        | `24000000` | System clock. The CH555 supports 12, 16, 24 and 32 MHz.                                                                                                                                |
| `MCS51_ISR_HEADERS`            |            | Headers that declare interrupt handlers. See [Interrupt handlers](#interrupt-handlers).                                                                                                |
| `MCS51_DCE`                    | `yes`      | Set to `no` to keep unused functions in the image.                                                                                                                                     |
| `MCS51_DCE_KEEP`               |            | Names of functions that only SDCC's library calls and that must not be removed. `putchar`, `getchar` and `_sdcc_external_startup` are always kept.                                     |
| `MCS51_FEWER_SPILLS_SRC`       |            | Source files compiled with fewer optimizations, which reduces their use of direct RAM. See [Memory](#memory). When VIA is enabled, `platform.mk` adds several core files to this list. |
| `CH555_EEPROM_SIZE`            | `1024`     | Size of QMK's EEPROM in code flash, in bytes (a multiple of 64). Applies with `EEPROM_DRIVER = vendor`.                                                                                |
| `CH555_EEPROM_CODE_FLASH_XRAM` | `0x1800`   | Address in xRAM of the EEPROM's working copy.                                                                                                                                          |
| `CH555_DATA_FLASH_ENABLE`      | `no`       | Compiles the driver for the CH555's 1 KB data flash (`data_flash.h`).                                                                                                                  |

## Writing code for SDCC

### Language

`platform.mk` passes `--std-sdcc11` to SDCC, which selects C11 with SDCC's extensions. SDCC rejects some C that GCC accepts: case ranges (`case 1 ... 5:`), casts to a union type, variable-length arrays, and empty structs. GCC attributes are removed by [sdcc_compat.h](../platforms/mcs51/sdcc_compat.h), which is included before every source file. That file also defines macros, such as `ARRAY_SIZE`, whose definitions in QMK's headers use GCC built-ins.

Print output goes through SDCC's own `printf()`, which writes to the MCU's UART through `putchar()`. QMK's `lib/printf` is not used.

A struct whose first member is an array must be zero-initialized with `{{0}}`. SDCC rejects `{0}` for it with error 69.

### Constructs that SDCC miscompiles

The driver stops the build at each of these.

| Construct                                                                                             | Write instead                                         |
| ----------------------------------------------------------------------------------------------------- | ----------------------------------------------------- |
| A struct or union returned from anything other than a variable, such as `return f();` or `return *p;` | Assign the value to a local variable and return that. |
| One struct or union variable returned by two `return` statements                                      | Return it from one statement.                         |
| A struct-valued `?:` expression with a function call as an operand                                    | `if`/`else`                                           |
| The address of a compound literal at file scope, `&(T){...}`                                          | A named object.                                       |
| An initializer that names a member of an anonymous struct or union                                    | Assign the members.                                   |
| Arithmetic on a `void *` (SDCC treats the element size as 0)                                          | A `uint8_t *`.                                        |

### Functions

Functions are not reentrant unless declared `__reentrant`. SDCC passes the second and later arguments, and keeps local variables, in fixed memory that belongs to the called function. This has three consequences:

- A function that is called both from an interrupt handler and from the main loop, or that calls itself, must be `__reentrant`.
- A function pointer that is called with more than one argument must be declared `__reentrant`, and so must the functions it points to, because at a call through a pointer, SDCC cannot determine which function's memory should receive the arguments. `send_raw_hid` in `host_driver.h` is an example.
- A `__naked` function that is defined in the same file as its caller must save every register it changes, except A, B, DPTR and PSW, because SDCC saves none around the call.

### Memory

`platform.mk` selects SDCC's large memory model, so variables are placed in external RAM (xRAM) unless declared otherwise. On the CH555 the linker places variables in 0x0100–0x0FFF. The USB buffers occupy 0x1000–0x13FF, and 0x1400–0x1FFF holds buffers at fixed addresses, such as the working copy of the EEPROM (`CH555_EEPROM_CODE_FLASH_XRAM`).

Direct RAM (0x08–0x7F) is small and shared by SDCC's spill locations (where it keeps values that do not fit in registers), bit variables and the stack. If new code adds spill locations, the link can fail, or too little stack can remain. Keep new variables in xRAM unless they are used on a time-critical path. When direct RAM runs out, add the files that use the most of it to `MCS51_FEWER_SPILLS_SRC`, which compiles them without some of SDCC's optimizations.

The linker's memory map, `<target>.mem` in the build directory, shows the end of code, the use of xRAM and where the stack starts. `qmk compile` prints these lines after a build.

### Interrupt handlers

SDCC adds a handler to the interrupt vector table only if `main.c` sees the handler's declaration. If a keyboard's code defines an interrupt handler, add the header that declares it to the keyboard's `rules.mk`:

```make
MCS51_ISR_HEADERS += my_handler.h
```

`platform.mk` adds each of these headers to the compiler options of `main.c` with `-include`.

Define the handler with the `MCS51_ISR(name, num)` macro from `mcs51_isr.h`; `num` is one of the `INT_NO_*` constants of the MCU. With the Evision bootloader, the vector table is part of the bootloader, which saves all registers itself and calls the application's handler as a plain function. Because of that, SDCC compiles the handler as an ordinary function, not as an interrupt handler. If the handler calls no other function, SDCC may place its local variables in memory that functions of the main loop also use. Enclose each handler, and every function it calls that has local variables in memory, in `#pragma save`, `#pragma nooverlay` and `#pragma restore`. The handlers in this tree do this regardless of the bootloader.

### Changes to the QMK core

Where SDCC cannot compile core code, the code is changed in one of two ways. A change that is also valid for GCC and has been submitted to QMK as a pull request is made without a guard, exactly as in the pull request. Any other change is placed under `#ifdef __SDCC`, with the original code in the `#else` branch and a comment at the guard that describes what SDCC does wrong and links the SDCC ticket.

### SDCC bug reports and feature requests

The following bugs and feature requests have workarounds in this tree, most of them guarded by `#ifdef __SDCC` or handled by the compiler driver:

- [Feature Request 595: Weak linkage](https://sourceforge.net/p/sdcc/feature-requests/595/)
- [Feature Request 1038: Compile case ranges as ranges instead of one case per value](https://sourceforge.net/p/sdcc/feature-requests/1038/)
- [Bug 1900: calling inline function](https://sourceforge.net/p/sdcc/bugs/1900/)
- [Bug 3857: Ternary operator in struct assignment using a function call fails](https://sourceforge.net/p/sdcc/bugs/3857/) (checked by the driver; no code in the build uses it)
- [Bug 3886: Compiler don't create constant for compound literal](https://sourceforge.net/p/sdcc/bugs/3886/) (fixed after 4.6.0, in r16747)
- [Bug 3979: error 147: excess elements in struct initializer](https://sourceforge.net/p/sdcc/bugs/3979/) (designated initializers into an anonymous struct: [comment 5099](https://sourceforge.net/p/sdcc/bugs/3979/#5099))
- [Bug 4044: Initializer loses const qualifier](https://sourceforge.net/p/sdcc/bugs/4044/) (fixed after 4.6.0, in r16763)
- [Bug 4157: Struct returned from a function call is not copied to the caller](https://sourceforge.net/p/sdcc/bugs/4157/)
- [Bug 4158: mcs51: second return of the same struct variable gives wrong values (large model)](https://sourceforge.net/p/sdcc/bugs/4158/)
- [Bug 4159: Repeated typedef of a struct or union tag gives error 141](https://sourceforge.net/p/sdcc/bugs/4159/)

## Source layout

The files are arranged as in the ChibiOS platform: code shared by all 8051 MCUs is in `platforms/mcs51/`, and code for one MCU family is in `platforms/mcs51/vendors/<MCU_FAMILY>/`.

| Path                              | Contents                                                                                                                                      |
| --------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| `platforms/mcs51/`                | Platform code: timer, GPIO interface, suspend, platform setup, the compiler driver and its helper files, MCU selection, bootloader selection. |
| `platforms/mcs51/vendors/WCH/`    | CH555 code: build rules (`CH555.mk`), the MCU interface (`mcs51_mcu.h`), defaults (`config.h`), port list, GPIO, chip ID, reset, data flash.  |
| `platforms/mcs51/drivers/eeprom/` | EEPROM drivers. `eeprom_ch555_code_flash.c` stores QMK's EEPROM in CH555 code flash.                                                          |
| `platforms/mcs51/bootloaders/`    | `bootloader_jump()` for each bootloader.                                                                                                      |
| `lib/ch555/`                      | WCH's register header and support code (clock, delays, UART).                                                                                 |
| `tmk_core/protocol/ch555/`        | USB stack for the CH555's USB controller.                                                                                                     |

### Build order and configuration

`builddefs/common_features.mk` is read first, then `platforms/mcs51/platform.mk`, which includes the vendor's `.mk`, and then the protocol's `.mk`. A vendor `.mk` therefore cannot test a variable that the protocol `.mk` sets. C defaults belong in a `config.h`, not in a conditional in a `.mk` file.

`platform.mk` adds `vendors/<MCU_FAMILY>/<MCU_SERIES>/config.h`, `vendors/<MCU_FAMILY>/config.h` and `platforms/mcs51/config.h` to the build, in that order, if they exist. They are included after the keyboard's and the keymap's `config.h`, so each default must be guarded with `#ifndef` so that a keyboard's `config.h` can override it.

The MCU's clock and print output are initialized in `platform_setup()`, through functions of `mcs51_mcu.h`.

### Pins

Pins are named `PORT<port>_<pin>`, for example `PORT4_0` for P4.0. A pin is stored in one byte: the port number in the upper five bits and the pin number in the lower three. The MCU's `_pin_defs.h` lists which ports exist. The 8051 can shift by a variable count only in a loop, so the pin mask is read from a table, `pin_to_pinmask`, which the MCU's `gpio.c` defines.

8051 ports have no pull-down resistors, so a call of `gpio_set_pin_input_low()` is a compile error.

### EEPROM

With `EEPROM_DRIVER = vendor`, `common_features.mk` selects the EEPROM driver by `MCU_SERIES`. For the CH555 this is the code flash driver: the EEPROM is a copy in xRAM, saved to one of two slots at the top of the application's code space 2 seconds after the last write. The vendor `.mk` computes the size and address of the slots and lowers `CODE_SIZE` so that code is never linked into them. For an MCU without its own driver, `common_features.mk` selects the transient driver, which keeps the EEPROM in RAM only.

## Adding an MCU

To port the platform to another 8051 MCU, add the following. The CH555 is the only MCU whose port is complete, so the list may be incomplete.

1. **MCU selection.** A block in `platforms/mcs51/mcu_selection.mk` that sets `PROTOCOL`, `MCU_FAMILY`, `MCU_SERIES`, `IRAM_SIZE`, `XRAM_LOC`, `XRAM_SIZE`, `CODE_SIZE` and a default `F_CPU`.
2. **Vendor directory.** `platforms/mcs51/vendors/<MCU_FAMILY>/` with:
    - `<MCU_SERIES>.mk`, or `<MCU_FAMILY>.mk` if one file serves the whole family. It adds the directory's sources to the build.
    - `mcs51_mcu.h`, which the platform's source files include. The CH555's copy is a short example. It must provide:
        - the interrupt enable bit `EA`;
        - `mcs51_clock_init()`, which sets the system clock to `F_CPU`;
        - `mcs51_print_init()`, which initializes the output that `putchar()` writes to; for an MCU without a print output, an `#error` if `NO_PRINT` is not defined;
        - `MCS51_TIMER_INIT()`, which starts a timer whose interrupt `INT_NO_TIMER` comes once per millisecond and enables that interrupt, and `MCS51_TIMER_RELOAD()`, which the handler runs on each tick (a reload, or the clearing of an overflow flag that the hardware does not clear);
        - `mcs51_delay_us(n)` and `mcs51_delay_ms(n)`, which wait at least `n` microseconds or milliseconds, for `n` up to 65535.
    - `_pin_defs.h`, the list of ports, and `gpio.c`, which implements the functions declared in `platforms/mcs51/gpio.h`. If the ports have no open-drain mode, `_pin_defs.h` defines `MCS51_NO_OPEN_DRAIN`, which makes `gpio_set_pin_output_open_drain()` a compile error.
    - `mcu_reset.c` and, if the MCU has a chip ID, `hardware_id.c`. Without it, the platform's default `get_hardware_id()` returns zeros; to send no serial number instead, define `SERIAL_NUMBER_USE_HARDWARE_ID FALSE` in the keyboard's `config.h`.
    - `config.h` with the MCU's defaults, such as `SERIAL_NUMBER_LENGTH`.

    The vendor's own register header and support code go in `lib/<mcu>/` if they are third-party code; a register header written from the datasheet goes in the vendor directory.

3. **EEPROM.** A driver for the MCU's flash in `platforms/mcs51/drivers/eeprom/` and an entry for its `MCU_SERIES` in `common_features.mk`. Without one, `EEPROM_DRIVER = vendor` results in the transient driver.
4. **USB.** A protocol in `tmk_core/protocol/<name>/`. Share code with the CH555 stack only after a second stack exists and it is clear which code the two have in common.
5. **Bootloader.** An entry in `platforms/mcs51/bootloader.mk`, a `bootloaders/<name>.c` with `bootloader_jump()`, and an image rule in `platform.mk` if the bootloader accepts a different image format than the linked binary.
6. **QMK CLI and schema.** The processor in `MCS51_PROCESSORS` and its default bootloader in `lib/python/qmk/constants.py`, and the processor and bootloader names in `data/schemas/keyboard.jsonschema`. `mcs51_processor_rules()` in `lib/python/qmk/info.py` sets the protocol to `CH555` for every MCS-51 processor and must be changed to choose it per processor.

Check the timer setup against the MCU's datasheet in particular. 8051 variants differ in their timers and in the clock that drives them, and the platform's 1 ms tick depends on the count per tick being correct.
