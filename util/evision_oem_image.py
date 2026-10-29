#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Make a flash image for the Evision OEM bootloader from an SDCC build.

The bootloader occupies 0x0000-0x27FF, starts the app at 0x2800, and forwards each interrupt to a table of LJMP entries
at 0x3000; the app and its EEPROM use 0x2800-0xEFBF. This script adds an Esc check that runs before the app: with Esc
held at plug-in, it erases the boot marker page 0x27C0 and resets, so the board starts in the bootloader.

Usage:
  python3 util/evision_oem_image.py sdcc APP.ihx MAIN.asm OUT.bin [--code-loc 0x3027] [--code-end 0xEFC0]
      APP.ihx is linked with --code-loc CODE_LOC and its code ends below CODE_END. MAIN.asm is the SDCC module
      with __interrupt_vect (quantum/main.asm in a QMK build).
  python3 util/evision_oem_image.py stock APP.bin OUT.bin
      Adds the Esc check to an app image based at 0x2800, such as stock firmware.
"""
import argparse, re

APP_BASE, ERASE_END = 0x2800, 0xEFC0
IVT_BASE = 0x3000
HW_VECTORS = [0x03, 0x0B, 0x13, 0x1B, 0x23, 0x2B, 0x33, 0x3B, 0x4B, 0x53, 0x5B, 0x63, 0x6B]  # 0x43 not forwarded

# SFRs / bits (lib/ch555/ch555.h)
EA, P0_0, P4_0 = 0xAF, 0x80, 0xC0
P0_MOD_OC, P0_DIR_PU, P4_LED_KEY, P4_MOD_OC = 0xC4, 0xC5, 0xC1, 0xC2
SAFE_MOD, GLOBAL_CFG = 0xA1, 0xB1
ROM_ADDR_L, ROM_ADDR_H, ROM_DATA_HL, ROM_DATA_HH, ROM_CTRL = 0x84, 0x85, 0x8E, 0x8F, 0x86
bCODE_WE, bSW_RESET = 0x08, 0x10
MARKER_PAGE = 0x27C0

def esc_check(base, cont):
    """Assemble the Esc check at address base; returns (bytes, ret_addr)."""
    code, labels, fix = bytearray(), {}, []
    def emit(*b): code.extend(b)
    def label(n): labels[n] = base + len(code)
    def rel(n): fix.append((len(code), n)); code.append(0)
    emit(0xC2, EA)                       # CLR EA
    # Save the mode registers changed below (restored before the jump to the app). At reset P4 is in
    # LED-drive mode, so key reads need P4_LED_KEY/P4_MOD_OC = 0xFF first (as in the stock firmware and in WCH's sleep
    # routine CH55X_Sleep_Deal).
    # (Mode registers only: reading P0/P4 returns pin levels, not the latch, so latches aren't saved;
    # the P4.0 latch is set to 1, and P0.0 is set to 1 again, its reset value.)
    for rn, sfr in enumerate((P4_LED_KEY, P4_MOD_OC, P0_MOD_OC, P0_DIR_PU)):
        emit(0xA8 + rn, sfr)             # MOV Rn,sfr    (R0..R3)
    emit(0x75, P4_LED_KEY, 0xFF)         # P4 -> key (GPIO input) mode
    emit(0x75, P4_MOD_OC, 0xFF)          # P4 open-drain + pull-up
    emit(0xD2, P4_0)                     # SETB P4.0     row 0 released
    emit(0x53, P0_MOD_OC, 0xFE)          # P0.0 push-pull ...
    emit(0x43, P0_DIR_PU, 0x01)          # ... output
    emit(0xC2, P0_0)                     # CLR P0.0      col 0 low
    emit(0x7E, 0x20)                     # MOV R6,#0x20  settle delay
    label("d1"); emit(0x7F, 0x00)        # MOV R7,#0
    label("d2"); emit(0xDF); rel("d2")   # DJNZ R7,d2
    emit(0xDE); rel("d1")                # DJNZ R6,d1
    emit(0x7D, 0x40)                     # MOV R5,#0x40  64 samples
    label("s"); emit(0x20, P4_0); rel("run")   # JB P4.0,run  (released -> normal boot)
    emit(0x7F, 0x00)                     # MOV R7,#0
    label("d3"); emit(0xDF); rel("d3")   # DJNZ R7,d3
    emit(0xDD); rel("s")                 # DJNZ R5,s
    # Esc held for the whole window: erase marker page, then software reset (stock sequence @0xdb9f).
    emit(0x75, SAFE_MOD, 0x55); emit(0x75, SAFE_MOD, 0xAA)
    emit(0x43, GLOBAL_CFG, bCODE_WE)     # ORL GLOBAL_CFG,#bCODE_WE
    emit(0x75, ROM_ADDR_H, MARKER_PAGE >> 8); emit(0x75, ROM_ADDR_L, MARKER_PAGE & 0xFF)
    emit(0x75, ROM_DATA_HH, 0x80); emit(0x75, ROM_DATA_HL, 0x00)
    emit(0xE5, ROM_CTRL)                 # MOV A,ROM_CTRL
    emit(0x30, 0xE6); rel("we_off")      # JNB ACC.6,we_off  (address not accepted -> skip)
    emit(0x75, ROM_CTRL, 0xA6)           # MOV ROM_CTRL,#0xA6  erase 64-byte page
    label("we_off")
    emit(0x75, SAFE_MOD, 0x55); emit(0x75, SAFE_MOD, 0xAA)
    emit(0x53, GLOBAL_CFG, 0xFF & ~bCODE_WE)   # ANL GLOBAL_CFG,#~bCODE_WE
    emit(0x75, SAFE_MOD, 0x55); emit(0x75, SAFE_MOD, 0xAA)
    emit(0x43, GLOBAL_CFG, bSW_RESET)    # ORL GLOBAL_CFG,#bSW_RESET  (does not return)
    label("halt"); emit(0x80); rel("halt")     # SJMP $
    label("run")                         # restore the saved port state, then run the app
    emit(0xD2, P0_0)                     # SETB P0.0
    emit(0x8B, P0_DIR_PU)                # MOV P0_DIR_PU,R3
    emit(0x8A, P0_MOD_OC)                # MOV P0_MOD_OC,R2
    emit(0x89, P4_MOD_OC)                # MOV P4_MOD_OC,R1
    emit(0x88, P4_LED_KEY)               # MOV P4_LED_KEY,R0
    emit(0x02, cont >> 8, cont & 0xFF)   # LJMP cont
    label("ret"); emit(0x22)             # RET (target for unused interrupt slots, called by the stub)
    for off, n in fix:
        d = labels[n] - (base + off + 1)
        if not -128 <= d <= 127:
            raise SystemExit("Esc check: branch to %s out of range" % n)
        code[off] = d & 0xFF
    return bytes(code), labels["ret"]

def ljmp(t): return bytes([0x02, t >> 8, t & 0xFF])

def place(img, addr, data):
    off = addr - APP_BASE
    if len(img) < off + len(data):
        img.extend(bytes(off + len(data) - len(img)))
    if any(img[off:off + len(data)]):
        raise SystemExit("refusing to overwrite non-zero bytes at 0x%04x" % addr)
    img[off:off + len(data)] = data

def finish(img, out):
    img = bytes(img).rstrip(b"\x00")
    end = APP_BASE + len(img)
    if end > ERASE_END:
        raise SystemExit("image ends at 0x%04x, past 0x%04x" % (end - 1, ERASE_END - 1))
    open(out, "wb").write(img)
    print("wrote %s: 0x%04x-0x%04x (%d bytes)" % (out, APP_BASE, end - 1, len(img)))

def do_stock(a):
    img = bytearray(open(a.app, "rb").read())
    if img[0] != 0x02:
        raise SystemExit("no LJMP at 0x2800")
    entry = (img[1] << 8) | img[2]
    last = max(i for i, b in enumerate(img) if b)
    base = APP_BASE + ((last + 64) // 64) * 64        # first free 64-byte page after the app
    code, _ = esc_check(base, entry)
    place(img, base, code)
    img[0:3] = ljmp(base)
    print("Esc check at 0x%04x (%d bytes), then LJMP to original entry 0x%04x" % (base, len(code), entry))
    finish(img, a.out)

def parse_ihx(path):
    """Return {address: byte}. Requires the end-of-file record, a length byte that matches each record, and
    no address in more than one record (a truncated or damaged .ihx would otherwise give a shorter image)."""
    mem = {}
    for line in open(path):
        line = line.strip()
        if not line.startswith(":"):
            continue
        try:
            rec = bytes.fromhex(line[1:])
        except ValueError:
            raise SystemExit("ihx: not a hex record: %s" % line)
        if len(rec) < 5 or len(rec) != rec[0] + 5:
            raise SystemExit("ihx: record length does not match its length byte: %s" % line)
        n, addr, typ = rec[0], (rec[1] << 8) | rec[2], rec[3]
        if (sum(rec) & 0xFF) != 0:
            raise SystemExit("ihx checksum error: %s" % line)
        if typ == 0:
            for i in range(n):
                if addr + i in mem:
                    raise SystemExit("ihx: address 0x%04x is in more than one record" % (addr + i))
                mem[addr + i] = rec[4 + i]
        elif typ == 1:
            return mem
        else:
            raise SystemExit("unsupported ihx record type %d" % typ)
    raise SystemExit("ihx: no end-of-file record (truncated file?): %s" % path)

def parse_vectors(asm):
    """Return list of SDCC vector slots (index n -> 'ljmp _X' symbol or None for reti)."""
    lines = open(asm).read().splitlines()
    i = next(k for k, l in enumerate(lines) if l.strip() == "__interrupt_vect:")
    slots, first = [], True
    for l in lines[i + 1:]:
        s = l.split(";")[0].strip()
        if not s or s.startswith(".ds"):
            continue
        m = re.match(r"(ljmp|reti)\b\s*(\S*)", s)
        if not m:
            break
        if first:
            first = False                     # reset vector: ljmp __sdcc_gsinit_startup
            continue
        slots.append(m.group(2) if m.group(1) == "ljmp" else None)
    return slots

def do_sdcc(a):
    code_loc = int(a.code_loc, 0)
    if code_loc < IVT_BASE + 3 * len(HW_VECTORS):
        raise SystemExit("code-loc must be >= 0x%04x" % (IVT_BASE + 3 * len(HW_VECTORS)))
    mem = parse_ihx(a.ihx)
    lo, hi = min(mem), max(mem)
    if lo != code_loc:
        raise SystemExit("ihx starts at 0x%04x, expected code-loc 0x%04x (link with --code-loc)" % (lo, code_loc))
    code_end = int(a.code_end, 0)
    if code_end > ERASE_END:
        raise SystemExit("code-end must be <= 0x%04x" % ERASE_END)
    if hi >= code_end:
        raise SystemExit("ihx ends at 0x%04x, past 0x%04x" % (hi, code_end - 1))
    slots = parse_vectors(a.main_asm)
    # cross-check SDCC's vector table in the binary
    if mem.get(code_loc) != 0x02:
        raise SystemExit("no LJMP at code-loc 0x%04x" % code_loc)
    for n, sym in enumerate(slots):
        b = mem.get(code_loc + 3 + 8 * n)
        if b != (0x02 if sym else 0x32):
            raise SystemExit("vector slot %d (%s) at 0x%04x is 0x%s" % (n, sym, code_loc + 3 + 8 * n,
                                                                       "%02x" % b if b is not None else "--"))
    img = bytearray(hi + 1 - APP_BASE)
    for addr, b in mem.items():
        img[addr - APP_BASE] = b
    cbase = APP_BASE + 3
    code, ret = esc_check(cbase, code_loc)
    if cbase + len(code) > IVT_BASE:
        raise SystemExit("Esc check too large")
    img[3:3 + len(code)] = code
    img[0:3] = ljmp(cbase)
    print("Esc check at 0x%04x (%d bytes), then LJMP to SDCC reset vector 0x%04x" % (cbase, len(code), code_loc))
    for j, v in enumerate(HW_VECTORS):
        n = (v - 3) // 8
        tgt = code_loc + v if n < len(slots) and slots[n] else ret
        img[IVT_BASE - APP_BASE + 3 * j:IVT_BASE - APP_BASE + 3 * j + 3] = ljmp(tgt)
        if tgt != ret:
            print("  vector 0x%02x (int %2d, %s) -> 0x3000+%2d: LJMP 0x%04x" % (v, n, slots[n], 3 * j, tgt))
    print("  all other vectors -> RET at 0x%04x" % ret)
    finish(img, a.out)

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("stock"); p.add_argument("app"); p.add_argument("out"); p.set_defaults(fn=do_stock)
    p = sub.add_parser("sdcc"); p.add_argument("ihx"); p.add_argument("main_asm"); p.add_argument("out")
    p.add_argument("--code-loc", default="0x3027")
    p.add_argument("--code-end", default="0x%04x" % ERASE_END); p.set_defaults(fn=do_sdcc)
    a = ap.parse_args()
    a.fn(a)

if __name__ == "__main__":
    main()
