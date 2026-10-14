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

Link (`--output ELF`):
  1. Weak symbols: a global defined in several objects, where every definition but one is weak, keeps the
     strong one: in each weak object, the definition's label (and the labels of its parameters, `_NAME_PARM_n`)
     is renamed, the global stays as a reference, and the object is assembled again. Its code stays in the image.
  2. Link with sdcc (the module with main() first) to ELF's name with .ihx, then convert the .ihx to ELF with
     objcopy, so that QMK's .hex/.bin rules work.

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
    rels = []
    for o in objs:
        b = o[:-2]
        rel = b + ".rel"
        if o in rename:
            wasm = b + ".weakened.asm"
            weaken(b + ".asm", wasm, rename[o], re.sub(r"\W", "_", os.path.basename(b)))
            run([SDAS, "-plosgffw", rel, wasm])
        else:
            if os.path.exists(rel):
                os.remove(rel)
            os.link(o, rel)
        rels.append(rel)
    with open(base + ".weak.txt", "w") as f:
        f.write("".join(r + "\n" for r in report))

    # Module with main() first
    main_rel = [r for o, r in zip(objs, rels) if "_main" in info[o][0]]
    if len(main_rel) != 1:
        die("main() must be defined once")
    rels.remove(main_rel[0])
    rels.insert(0, main_rel[0])
    for ext in (".ihx", ".map", ".mem"):
        if os.path.exists(base + ext):
            os.remove(base + ext)
    run([SDCC] + args.cc + rels + ["-o", ihx])
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
