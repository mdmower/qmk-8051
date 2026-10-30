#!/usr/bin/env python3
# Copyright 2026 QMK
# SPDX-License-Identifier: GPL-2.0-or-later
"""GCC-style compiler driver for SDCC (MCS-51), used as $(CC) by QMK's build rules.

QMK's rules call the compiler as GCC: `-c SRC -o OBJ` with `-include`, `-MMD -MP -MF`, and the link as
`OBJ... --output ELF`. This driver runs SDCC for these. Where SDCC does not take QMK's C, the QMK core is edited
(standard C, or `#ifdef __SDCC`); two things that need a C compiler's view of the source are done with clang,
which reads the same preprocessed text as SDCC:

Compile (`-c`):
  1. Preprocess with sdcpp (`sdcc -E`) with GCC attributes kept (QMK_SDCC_ATTRIBUTES; sdcc_compat.h removes them
     for SDCC) to OBJ.clang.i; the dependency file is written here.
  2. Weak symbols: clang (target AVR: 16-bit int, 1-byte alignment, as SDCC's MCS-51 code; SDCC's keywords from
     sdcc_clang.h) compiles OBJ.clang.i to LLVM bitcode; its weak definitions (llvm-nm: W, V) are listed in
     OBJ.weak.
  3. clang-query (sdcc_check.query) on OBJ.clang.i: an error for each construct that SDCC 4.6.0 compiles to wrong
     code without a diagnostic.
  4. Compile the source to OBJ.asm (`sdcc -S`). A file with weak definitions is compiled with --nopurity:
     otherwise SDCC drops calls to an empty function defined in the same file, and the weak definition would
     replace the one linked from another file.
  5. Assemble OBJ.asm to OBJ (sdas8051; SDCC's .rel format).
  A file with SDCC inline assembly (`__asm` ... `__endasm`, which clang cannot read) skips steps 2 and 3: its
  definitions count as strong, and it is not checked.
  With --codeseg-per-object the code of each object is its own area, CSEG_ and the object's path, so that the
  linker places the objects' code around absolute code areas (an MCU's reserved addresses, in its vendor files)
  instead of one code area that must fit between them. The link then orders the objects (step 4).

Link (`--output ELF`):
  1. Weak symbols: a global defined in several objects, where every definition but one is weak, keeps the
     strong one: in each weak object, the definition's label (and the labels of its parameters, `_NAME_PARM_n`)
     is renamed, the global stays as a reference, and the object is assembled again. Step 2 removes its code
     (not with --no-dce).
  2. Unused functions (as GCC's --gc-sections; SDCC links every function of an object): functions that no
     reachable code or data names are removed from the objects' .asm, with the locals and parameters SDCC lists
     for them that nothing names any more (`dce` below; --no-dce turns it off, --dce-keep=NAME keeps the C
     function NAME as SDCC's library would call it). The list is written to ELF's name with .dce.txt. Every
     object is assembled again from the .asm the link uses, so that each .lst, and the linker's .rst made from
     it, matches the linked code.
  3. Link with sdcc (the module with main() first) to ELF's name with .ihx, then convert the .ihx to ELF with
     objcopy, so that QMK's .hex/.bin rules work. An undefined global stops the link (sdld only warns).
  4. With per-object code areas and absolute code areas: sdld places the relocatable areas one after another in
     link order; an area that would overlap an absolute area goes past it, to an address up to 31 bytes after
     its end (sdld scans a bitmap in 32-bit words), which leaves the space before the absolute area unused. So
     the link runs twice: the first time with the code size relaxed to 64 KB, only for the end of the areas that
     precede the objects' (HOME, GSINIT, GSFINAL, CSEG, CONST, XINIT, main's) in its map; then the objects are
     ordered so that each space is filled with the largest areas that fit (`pack_code_areas`, area sizes from
     the .rel files; after a skipped absolute area the next address is taken as 32 bytes after its end, which
     the real one never exceeds), and the link runs again with the code size given.

Other GCC options that SDCC does not have (-W, -O, -f, -g, -std, -l, -Wl) are ignored; the platform sets SDCC's
own options (platform.mk). clang, clang-query and llvm-nm: CLANG, CLANG_QUERY, LLVM_NM.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SDCC = os.environ.get("SDCC", "sdcc")
SDAS = os.environ.get("SDAS", "sdas8051")
OBJCOPY = os.environ.get("OBJCOPY_SDCC", "objcopy")
CLANG = os.environ.get("CLANG", "clang")
CLANG_QUERY = os.environ.get("CLANG_QUERY", "clang-query")
LLVM_NM = os.environ.get("LLVM_NM", "llvm-nm")
CLANG_FLAGS = ["--target=avr", "-x", "c", "-std=gnu11", "-w", "-include", os.path.join(HERE, "sdcc_clang.h")]
CHECKS = {
    "sdcc-struct-return": "struct or union returned from an expression other than a variable (assign it to a "
                          "local variable and return that)",
    "sdcc-struct-return-twice": "one struct or union variable returned by two return statements (return it once)",
    "sdcc-struct-conditional": "struct-valued ?: with a call operand (use if/else)",
    "sdcc-static-compound-literal": "address of a compound literal at file scope (use a named object)",
    "sdcc-anonymous-member-init": "initializer that names a member of an anonymous struct or union (assign the "
                                  "members)",
    "sdcc-void-pointer-arithmetic": "arithmetic on a void pointer (use a uint8_t pointer)",
}

# SDCC options that take a separate value
VALUE_OPTS = {
    "--iram-size", "--xram-size", "--xram-loc", "--code-size", "--code-loc", "--stack-loc", "--stack-size",
    "--data-loc", "--idata-loc", "--xstack-loc", "--disable-warning", "--include", "--codeseg", "--constseg",
    "--dataseg", "--max-allocs-per-node",
}
# Options of the preprocessor (only given to step 1 of a compile)
PP_PREFIX = ("-D", "-U", "-I")


def die(msg):
    sys.stderr.write("sdcc_cc: " + msg + "\n")
    sys.exit(1)


def run(cmd, **kw):
    r = subprocess.run(cmd, **kw)
    if r.returncode != 0:
        sys.exit(r.returncode)
    return r


class Args:
    def __init__(self, argv):
        self.mode = "link"
        self.inputs = []
        self.output = None
        self.pp = []  # preprocessor options
        self.cc = []  # code generation options
        self.dep = None  # (file, phony)
        self.dce = True
        self.dce_keep = []
        self.codeseg_per_object = False
        dep_file, dep_phony, dep_mmd = None, False, False
        i = 0
        while i < len(argv):
            a = argv[i]
            nxt = argv[i + 1] if i + 1 < len(argv) else None
            if a in ("-c", "-S", "-E"):
                self.mode = {"-c": "compile", "-S": "asm", "-E": "preprocess"}[a]
            elif a in ("-o", "--output"):
                self.output = nxt
                i += 1
            elif a == "-include" or a == "--include":
                self.pp += ["--include", nxt]
                i += 1
            elif a in ("-D", "-U", "-I"):
                self.pp += [a + nxt]
                i += 1
            elif a.startswith(PP_PREFIX) or a in ("-dM", "-dD"):
                self.pp.append(a)
            elif a in ("-MMD", "-MD"):
                dep_mmd = True
            elif a == "-MP":
                dep_phony = True
            elif a == "-MF":
                dep_file = nxt
                i += 1
            elif a == "-x":
                i += 1  # C only
            elif a == "--no-dce":
                self.dce = False
            elif a == "--codeseg-per-object":
                self.codeseg_per_object = True
            elif a.startswith("--dce-keep="):
                self.dce_keep.append(a.split("=", 1)[1])
            elif a in VALUE_OPTS:
                self.cc += [a, nxt]
                i += 1
            elif a.startswith("-m") and not a.startswith("-mmcu"):
                self.cc.append(a)
            elif a.startswith("--"):
                self.cc.append(a)
            elif a.startswith(("-W", "-O", "-f", "-g", "-std", "-l", "-L", "-save-temps", "-mmcu", "-p", "-v", "-H")):
                pass  # GCC options without an SDCC equivalent
            elif a.startswith("-"):
                die("unknown option " + a)
            else:
                self.inputs.append(a)
            i += 1
        if dep_mmd and dep_file:
            self.dep = (dep_file, dep_phony)


# ---------------------------------------------------------------------------------------------------------------
# Compile
# ---------------------------------------------------------------------------------------------------------------
def tool(cmd, **kw):
    try:
        return subprocess.run(cmd, **kw)
    except FileNotFoundError:
        die("%s not found: the MCS-51 build needs clang, clang-query and llvm-nm (LLVM)" % cmd[0])


def clang_analysis(src, ana, base):
    """Weak definitions of the preprocessed file ana (clang, llvm-nm); dies at a construct of sdcc_check.query."""
    bc = base + ".clang.bc"
    r = tool([CLANG] + CLANG_FLAGS + ["-emit-llvm", "-c", ana, "-o", bc], stderr=subprocess.PIPE)
    if r.returncode != 0:
        die("%s: clang cannot read the preprocessed file %s:\n%s" % (src, ana, r.stderr.decode(errors="replace")))
    r = tool([LLVM_NM, "--defined-only", "-P", bc], stdout=subprocess.PIPE)
    if r.returncode != 0:
        sys.exit(r.returncode)
    weak = set()
    for line in r.stdout.decode().splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[1] in ("W", "V"):
            weak.add(parts[0])
    r = tool([CLANG_QUERY, "-f", os.path.join(HERE, "sdcc_check.query"), ana, "--"] + CLANG_FLAGS,
             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    found = []
    for line in r.stdout.decode(errors="replace").splitlines():
        m = re.match(r'^(.*?:\d+:\d+): note: "(sdcc-[a-z-]+)" binds here', line)
        if m and m.group(2) in CHECKS:
            found.append("%s: %s" % (m.group(1), CHECKS[m.group(2)]))
    if r.returncode != 0 and not found:
        die("%s: clang-query failed:\n%s" % (src, r.stdout.decode(errors="replace")[-2000:]))
    if found:
        die("%s: SDCC 4.6.0 compiles these to wrong code:\n  %s" % (src, "\n  ".join(sorted(set(found)))))
    return weak


def code_area_name(obj):
    """CSEG_ and the object's path below the build's object directory (the last obj_* component), with non-word
    characters as _."""
    parts = os.path.normpath(obj[:-2] if obj.endswith(".o") else obj).split(os.sep)
    for i in range(len(parts) - 1, -1, -1):
        if parts[i].startswith("obj_"):
            parts = parts[i + 1:]
            break
    return "CSEG_" + re.sub(r"\W", "_", "_".join(parts))


def compile_c(args):
    if len(args.inputs) != 1 or not args.output:
        die("compile needs one input and -o")
    src, obj = args.inputs[0], args.output
    base = obj[:-2] if obj.endswith(".o") else obj
    ana = base + ".clang.i"
    cmd = [SDCC, "-E", "-DQMK_SDCC_ATTRIBUTES"] + args.cc + args.pp + [src]
    if args.dep:
        dep_file, phony = args.dep
        cmd.insert(2, "-Wp,-MMD,-MF,%s,-MT,%s%s" % (dep_file, obj, ",-MP" if phony else ""))
    r = subprocess.run(cmd, stdout=subprocess.PIPE)
    if r.returncode != 0:
        sys.exit(r.returncode)
    with open(ana, "wb") as f:
        f.write(r.stdout)
    weak = set()
    if b"__endasm" not in r.stdout:
        weak = clang_analysis(src, ana, base)
    weak_file = base + ".weak"
    if weak:
        with open(weak_file, "w") as f:
            f.write("\n".join(sorted(weak)) + "\n")
    elif os.path.exists(weak_file):
        os.remove(weak_file)
    cc = list(args.cc)
    if weak:
        cc.append("--nopurity")
    if args.codeseg_per_object:
        cc += ["--codeseg", code_area_name(obj)]
    asm = base + ".asm"
    run([SDCC, "-S"] + cc + args.pp + [src, "-o", asm])
    if args.mode == "asm":
        return
    run([SDAS, "-plosgffw", obj, asm])


def preprocess(args):
    r = run([SDCC, "-E"] + args.cc + args.pp + args.inputs, stdout=subprocess.PIPE)
    if args.output and args.output != "-":
        with open(args.output, "wb") as f:
            f.write(r.stdout)
    else:
        sys.stdout.buffer.write(r.stdout)


# ---------------------------------------------------------------------------------------------------------------
# Link
# ---------------------------------------------------------------------------------------------------------------
LABEL_RE = re.compile(r"^([A-Za-z_$.][A-Za-z0-9_$.]*)::?(?=\s|$|;)")
GLOBL_RE = re.compile(r"^\s*\.globl\s+([A-Za-z_$.][A-Za-z0-9_$.]*)")


def asm_globals(asm_path):
    """Global symbols defined in an SDCC .asm: names in .globl that are also labels in the file."""
    globs, labels = set(), set()
    with open(asm_path, encoding="utf-8", errors="surrogateescape") as f:
        for line in f:
            m = GLOBL_RE.match(line)
            if m:
                globs.add(m.group(1))
                continue
            m = LABEL_RE.match(line)
            if m:
                labels.add(m.group(1))
    return globs & labels


def weaken(asm_in, asm_out, names, tag):
    """Renames the labels of names in asm_in; .globl lines stay, so references resolve to the strong definition."""
    pat = re.compile(r"^(%s)(::?)" % "|".join(re.escape(n) for n in sorted(names, key=len, reverse=True)))
    with open(asm_in, encoding="utf-8", errors="surrogateescape") as f:
        lines = f.readlines()
    with open(asm_out, "w", encoding="utf-8", errors="surrogateescape") as f:
        for line in lines:
            line = pat.sub(lambda m: "%s__weak_%s:" % (m.group(1), tag), line, count=1)
            f.write(line)


# Unused functions. A function is the code area text (CSEG, or CSEG_<object> with --codeseg-per-object) from its
# label (the first label after SDCC's `; function NAME` comment) to the next function's label or the next .area
# line. It is kept when a kept function or anything
# outside a function names it (calls, jumps, `mov dptr,#_f`, address tables, the interrupt vectors, GSINIT) or
# one of the labels in its text. A function whose text does not end in a return or a jump (inline assembly, a
# table), and code outside functions that does not, keep the function after them, into which they can run.
# The data labels that SDCC's allocation comments list for a removed function go too, when nothing names them
# and they only reserve space (.ds) in an area of uninitialized data. A removed function that is still named
# leaves an undefined global, which stops the link (the assembler takes undefined symbols as external). Functions
# that only SDCC's library calls are not named in the objects: DCE_KEEP and --dce-keep keep them.
DCE_KEEP = ("putchar", "getchar", "_sdcc_external_startup")
FUNC_RE = re.compile(r"^;\s+function (\S+)\s*$")
AREA_RE = re.compile(r"^\s*\.area\s+(\w+)")
CODE_AREA_RE = re.compile(r"^CSEG(_|$)")
NAMED_LABEL_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)::?(?=\s|$)")
EQU_RE = re.compile(r"^\s*[A-Za-z_][A-Za-z0-9_]*\s*=")
SYM_RE = re.compile(r"(?<![\w$.])(_[A-Za-z0-9_]+)")
END_RE = re.compile(r"^\s*(ret|reti|ljmp|sjmp|ajmp|jmp)\b")
ALLOC_HDR_RE = re.compile(r"^;Allocation info for local variables in function '(\w+)'")
ALLOC_NAME_RE = re.compile(r"^;.*Allocated with name '(_\w+)'")
DS_RE = re.compile(r"^\s*\.ds\s+\w+\s*$")
# Uninitialized data. Not XISEG, whose objects the startup code fills from XINIT in the same order.
DATA_AREAS = {"DSEG", "OSEG", "BSEG", "XSEG", "ISEG", "PSEG"}


def code_part(line):
    return line.split(";", 1)[0]


def named_syms(c):
    """Symbols named by the code part c of a line, without a label it defines."""
    m = NAMED_LABEL_RE.match(c)
    if m:
        c = c[m.end():].lstrip(":")
    return set(SYM_RE.findall(c))


class AsmModule:
    def __init__(self, obj, lines):
        self.obj = obj
        self.lines = lines
        self.funcs = {}  # label -> [start, end) line range
        self.owner = {}  # label in a function's text (its own label too) -> function label
        self.refs = {}  # function label -> symbols its text names
        self.falls = {}  # function label -> label of the function it can run into
        self.top = set()  # symbols named outside functions
        self.alloc = {}  # function label -> data labels SDCC lists for its locals, parameters and spill locations
        self.globl = set()
        alloc_by_name, alloc_fn = {}, None
        area, header, cur, last, top_last = None, None, None, None, None
        in_code = False
        ends, order = {}, []
        for i, line in enumerate(lines):
            m = ALLOC_HDR_RE.match(line)
            if m:
                alloc_fn = m.group(1)
                alloc_by_name.setdefault(alloc_fn, set())
                continue
            m = ALLOC_NAME_RE.match(line)
            if m and alloc_fn:
                alloc_by_name[alloc_fn].add(m.group(1))
                continue
            m = FUNC_RE.match(line)
            if m:
                header = m.group(1)
                continue
            c = code_part(line)
            if not c.strip():
                continue
            m = AREA_RE.match(c)
            label = NAMED_LABEL_RE.match(c)
            if m or (label and in_code and header is not None):
                if cur is not None:
                    self.funcs[cur][1] = i
                    ends[cur] = last is not None and bool(END_RE.match(last))
                    if m and not ends[cur]:
                        die("%s: function %s runs into the end of its code area" % (obj, cur))
                cur = None
            if m:
                area, top_last = m.group(1), None
                in_code = bool(CODE_AREA_RE.match(area))
                order.append(None)
                continue
            m = GLOBL_RE.match(c)
            if m:
                self.globl.add(m.group(1))
                continue
            if label and in_code and header is not None:
                cur, last = label.group(1), None
                if top_last is not None and not END_RE.match(top_last):
                    self.top.add(cur)
                top_last = None
                self.funcs[cur] = [i, len(lines)]
                self.owner[cur] = cur
                self.refs[cur] = set()
                self.alloc[cur] = alloc_by_name.get(header, set())
                header = None
                order.append(cur)
                continue
            if EQU_RE.match(c):
                continue
            if cur is not None:
                if label:
                    self.owner[label.group(1)] = cur
                rest = c[label.end():].lstrip(":") if label else c
                if rest.strip():
                    last = rest
                self.refs[cur] |= named_syms(c)
            else:
                self.top |= named_syms(c)
                if in_code:
                    rest = c[label.end():].lstrip(":") if label else c
                    if rest.strip():
                        top_last = rest
        if cur is not None:
            ends[cur] = last is not None and bool(END_RE.match(last))
            if not ends[cur]:
                die("%s: function %s runs into the end of its code area" % (obj, cur))
        for a, b in zip(order, order[1:]):
            if a is not None and b is not None and not ends[a]:
                self.falls[a] = b


def dce(mods, keep):
    """Removes unreachable functions and their unused data from mods, keeping main and the C functions keep;
    returns {obj: lines} and a report."""
    glob = {}
    for m in mods:
        for label, fn in m.owner.items():
            if label in m.globl:
                glob[label] = (m, fn)

    def resolve(m, s):
        if s in m.owner:
            return (m, m.owner[s])
        return glob.get(s)

    live, work = set(), []

    def mark(m, s):
        r = resolve(m, s)
        if r and (r[0].obj, r[1]) not in live:
            live.add((r[0].obj, r[1]))
            work.append(r)

    for m in mods:
        for s in m.top:
            mark(m, s)
    for name in ("main",) + tuple(keep):
        if "_" + name in glob:
            mark(glob["_" + name][0], "_" + name)
    while work:
        m, fn = work.pop()
        for s in m.refs[fn]:
            mark(m, s)
        if fn in m.falls:
            mark(m, m.falls[fn])

    report, killed, dead_alloc = [], {}, {}
    for m in mods:
        dead = [fn for fn in m.funcs if (m.obj, fn) not in live]
        killed[m.obj] = set()
        dead_alloc[m.obj] = set()
        if not dead:
            continue
        drop = set()
        for fn in dead:
            drop.update(range(*m.funcs[fn]))
            dead_alloc[m.obj] |= m.alloc[fn]
            report.append("%s: %s" % (m.obj, fn[1:]))
        for i in drop:
            killed[m.obj] |= set(SYM_RE.findall(code_part(m.lines[i])))
        m.lines = [line for i, line in enumerate(m.lines) if i not in drop or EQU_RE.match(code_part(line))]

    # Data of removed functions that nothing names any more (a weak definition's renamed labels too)
    named = set()
    for m in mods:
        for line in m.lines:
            c = code_part(line)
            if not GLOBL_RE.match(c):
                named |= named_syms(c)
    for m in mods:
        cands = dead_alloc[m.obj]
        if not cands:
            continue
        out, area, i, L = [], None, 0, m.lines
        while i < len(L):
            c = code_part(L[i])
            a = AREA_RE.match(c)
            if a:
                area = a.group(1)
            label = NAMED_LABEL_RE.match(c)
            if label and area in DATA_AREAS and label.group(1) not in named and \
                    (label.group(1) in cands or label.group(1).split("__weak_")[0] in cands) and \
                    not c[label.end():].lstrip(":").strip():
                j = i + 1
                while j < len(L) and (DS_RE.match(code_part(L[j])) or not L[j].strip()):
                    j += 1
                if j > i + 1:
                    killed[m.obj].add(label.group(1))
                    report.append("%s: %s (data)" % (m.obj, label.group(1)))
                    i = j
                    continue
            out.append(L[i])
            i += 1
        m.lines = out

    # .globl lines of symbols that a removed line named (in any module; for a weak definition, its renamed label
    # stands for the global) and that the module neither defines nor names. Other .globl lines stay (main's
    # module names the C startup modules of SDCC's library this way).
    gone = set()
    for k in killed.values():
        gone |= k | {s.split("__weak_")[0] for s in k}
    changed = {}
    for m in mods:
        defined, used = set(), set()
        for line in m.lines:
            c = code_part(line)
            label = NAMED_LABEL_RE.match(c)
            if label:
                defined.add(label.group(1))
            if not GLOBL_RE.match(c):
                used |= named_syms(c)
        out = []
        for line in m.lines:
            g = GLOBL_RE.match(code_part(line))
            if g and g.group(1) in gone and g.group(1) not in defined and g.group(1) not in used:
                continue
            out.append(line)
        if out != m.lines or killed[m.obj]:
            changed[m.obj] = out
    return changed, report


REL_AREA_RE = re.compile(r"^A (\S+) size ([0-9A-Fa-f]+) flags ([0-9A-Fa-f]+) addr ([0-9A-Fa-f]+)")
MAP_AREA_RE = re.compile(r"^(\S+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})\s+=\s+\d+\. bytes \((ABS|REL),CON,CODE\)")
AREA_ABS, AREA_CODE = 0x08, 0x20


def rel_areas(rel):
    """{area: (size, flags, addr)} from the header of a .rel file"""
    areas = {}
    with open(rel, encoding="utf-8", errors="surrogateescape") as f:
        for line in f:
            m = REL_AREA_RE.match(line)
            if m:
                areas[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16))
            elif line.startswith("T "):
                break
    return areas


def code_layout(rels):
    """(holes, items, main_area) of the link of rels: the absolute code areas as (start, end), the other
    objects' code areas (CSEG_*) as (size, name, rel), the first object's code area; None when there is no
    absolute code area or no per-object area."""
    holes, items, main_area = [], [], None
    for i, rel in enumerate(rels):
        for name, (size, flags, addr) in rel_areas(rel).items():
            if flags & AREA_CODE and flags & AREA_ABS and size:
                holes.append((addr, addr + size))
            elif CODE_AREA_RE.match(name) and name != "CSEG":
                if i == 0:
                    main_area = name
                else:
                    items.append((size, name, rel))
    if not holes or not items:
        return None
    return sorted(set(holes)), items, main_area


# The address sdld gives an area that it moves past an absolute area is at most this far after the area's end
SKIP_SLACK = 32


def pack_code_areas(rels, layout, map_path):
    """Link order of rels (the first stays first) in which the objects' code areas fill the code space before
    each absolute code area, from layout (code_layout) and the first link's map."""
    holes, items, main_area = layout
    items = list(items)
    prefix = {"HOME", "GSFINAL", "CSEG", "CONST", "XINIT", main_area}
    end = 0
    with open(map_path, encoding="utf-8", errors="surrogateescape") as f:
        for line in f:
            m = MAP_AREA_RE.match(line)
            if m and (m.group(1) in prefix or m.group(1).startswith("GSINIT")) and m.group(4) == "REL":
                end = max(end, int(m.group(2), 16) + int(m.group(3), 16))
    order, cur = [], end
    items.sort(key=lambda it: (-it[0], it[1]))
    while items:
        ahead = [h for h in holes if h[1] + SKIP_SLACK > cur]
        if ahead and ahead[0][0] <= cur:
            cur = ahead[0][1] + SKIP_SLACK
            continue
        space = ahead[0][0] - cur if ahead else None
        pick = next((it for it in items if space is None or it[0] <= space), None)
        if pick is None:
            cur = ahead[0][1] + SKIP_SLACK
            continue
        items.remove(pick)
        order.append(pick[2])
        cur += pick[0]
    return [rels[0]] + order


def link(args):
    if not args.output:
        die("link needs --output")
    objs = []
    for o in args.inputs:
        if o not in objs:
            objs.append(o)
    for o in objs:
        if not o.endswith(".o"):
            die("cannot link %s (SDCC links only objects compiled by this driver)" % o)
    out = args.output
    base = out[:-4] if out.endswith(".elf") else out
    ihx = base + ".ihx"

    # Global definitions per object, weak names per object
    defs = {}
    info = {}
    for o in objs:
        b = o[:-2]
        g = asm_globals(b + ".asm")
        w = set()
        if os.path.exists(b + ".weak"):
            with open(b + ".weak") as f:
                w = {"_" + n.strip() for n in f if n.strip()}
        info[o] = (g, w)
        for s in g:
            defs.setdefault(s, []).append(o)

    # Weak resolution
    rename = {}
    report = []
    for s, where in sorted(defs.items()):
        if len(where) < 2:
            continue
        m = re.match(r"^(.*)_PARM_\d+$", s)
        if m and m.group(1) in defs:
            continue  # handled with its function
        strong = [o for o in where if s not in info[o][1]]
        weak = [o for o in where if s in info[o][1]]
        if len(strong) > 1:
            die("%s is defined in %s" % (s, ", ".join(strong)))
        keep = strong[0] if strong else weak[0]
        for o in where:
            if o == keep:
                continue
            names = {s} | {p for p in info[o][0] if re.match(re.escape(s) + r"_PARM_\d+$", p)}
            rename.setdefault(o, set()).update(names)
        report.append("%s: %s" % (s[1:], keep))
    asm = {}
    for o in objs:
        b = o[:-2]
        asm[o] = b + ".asm"
        if o in rename:
            asm[o] = b + ".weakened.asm"
            weaken(b + ".asm", asm[o], rename[o], re.sub(r"\W", "_", os.path.basename(b)))
    with open(base + ".weak.txt", "w") as f:
        f.write("".join(r + "\n" for r in report))
    report = []
    if args.dce:
        mods = []
        for o in objs:
            with open(asm[o], encoding="utf-8", errors="surrogateescape") as f:
                mods.append(AsmModule(o, f.readlines()))
        out_lines, report = dce(mods, DCE_KEEP + tuple(args.dce_keep))
        for o, lines in out_lines.items():
            asm[o] = o[:-2] + ".dce.asm"
            with open(asm[o], "w", encoding="utf-8", errors="surrogateescape") as f:
                f.writelines(lines)
    for o in objs:
        if asm[o] != o[:-2] + ".dce.asm" and os.path.exists(o[:-2] + ".dce.asm"):
            os.remove(o[:-2] + ".dce.asm")  # of an earlier link
    with open(base + ".dce.txt", "w") as f:
        f.write("".join(r + "\n" for r in report))
    rels = []
    for o in objs:
        rel = o[:-2] + ".rel"
        # The .rel of an earlier link can be a hard link to the object: remove it, so that sdas8051 does not
        # write into the object file.
        if os.path.exists(rel):
            os.remove(rel)
        run([SDAS, "-plosgffw", rel, asm[o]])
        rels.append(rel)

    # Module with main() first
    main_rel = [r for o, r in zip(objs, rels) if "_main" in info[o][0]]
    if len(main_rel) != 1:
        die("main() must be defined once")
    rels.remove(main_rel[0])
    rels.insert(0, main_rel[0])
    def sdld(order, cc):
        for ext in (".ihx", ".map", ".mem"):
            if os.path.exists(base + ext):
                os.remove(base + ext)
        r = subprocess.run([SDCC] + cc + order + ["-o", ihx], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        log = r.stdout.decode(errors="replace")
        if r.returncode != 0:
            sys.stdout.write(log)
            sys.exit(r.returncode)
        if "Undefined Global" in log:
            os.remove(ihx)
            sys.stdout.write(log)
            die("undefined global symbols (above)")
        if "?ASlink-Error" in log:
            os.remove(ihx)
            sys.stdout.write(log)
            die("linker error (above)")
        return log

    layout = code_layout(rels)
    if layout is not None:
        relaxed = list(args.cc)
        if "--code-size" in relaxed:
            relaxed[relaxed.index("--code-size") + 1] = "0x10000"
        sdld(rels, relaxed)
        rels = pack_code_areas(rels, layout, base + ".map")
    sys.stdout.write(sdld(rels, args.cc))
    run([OBJCOPY, "-I", "ihex", "-O", "elf32-little", ihx, out])


def main(argv):
    if argv[:1] in (["--version"], ["-v"]):
        r = subprocess.run([SDCC, "--version"], stdout=subprocess.PIPE)
        sys.stdout.write(r.stdout.decode().splitlines()[0] + "\n")
        return
    if "/dev/null" in argv:
        sys.exit(1)  # option probes (builddefs/support.mk cc-option): no GCC options are supported
    args = Args(argv)
    if args.mode in ("compile", "asm"):
        compile_c(args)
    elif args.mode == "preprocess":
        preprocess(args)
    else:
        link(args)


if __name__ == "__main__":
    main(sys.argv[1:])
