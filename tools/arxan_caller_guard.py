"""Which launches hang when a function is called from outside the game image?

    python tools/arxan_caller_guard.py <function-rva> [...] [--image <dump>]

<dump> is a memory-layout image of BlackOps3.exe (file offset == RVA, e.g.
written from a running game); default: BlackOps3_06531394_UNPACKED.exe next
to the tools folder. RVAs of build 0x06531394 in the examples.

Many engine functions open with an Arxan caller check: bits 12..15 of the PEB
address (random per launch) pick one of 16 variants from a jump table; each
tests the function's own return address - inside the image
(< image+0x20000000, >= image base) or a call instruction before it - and
drives a flattened state machine (edx ^= ecx per state) that either reaches
the function body or cycles forever. This follows only that control flow:
the first memory read in a variant is taken as the return address, the
bytes before it as an `ff`/`e8` call, everything else is ignored.

Per function: the variants that hang for a caller ABOVE the image (ezz's
boiii.exe, the mod's DLL), BELOW it, and inside it (must be empty - if not,
the "first read = return address" assumption is wrong for that function).
Built 2026-09-29 for the ezz ClientCommand hang (LOG.md, same date).
"""
import os, re, struct, sys
from capstone import *
from capstone.x86 import *
ARGS = sys.argv[1:]
IMAGE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "BlackOps3_06531394_UNPACKED.exe")
if "--image" in ARGS:
    i = ARGS.index("--image")
    IMAGE = ARGS[i + 1]
    del ARGS[i:i + 2]
IMG = open(IMAGE, "rb").read()
md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
BASE = 0x7FF694260000   # any load address; only differences to it matter
def ins_at(a):
    return next(md.disasm(IMG[a:a + 16], a))
def guard(f):
    s = IMG.find(b"\x65\x48\x8B\x04\x25\x30\x00\x00\x00", f, f + 0x200)
    exitp = None
    for st in (f + 5, f):
        ins = list(md.disasm(IMG[st:s], st))
        if ins and ins[-1].address + ins[-1].size == s:
            for i in ins:
                if i.mnemonic == "je":
                    exitp = i.operands[0].imm
            break
    tbl = None; a = s
    for i in md.disasm(IMG[s:s + 0x60], s):
        if i.mnemonic == "mov" and "*4 +" in i.op_str:
            tbl = int(re.search(r"\+ (0x[0-9a-f]+)\]", i.op_str).group(1), 16); break
    return s, exitp, tbl
def run(entry, ret, retbytes, EXIT):
    R = {}; fl = {}; pc = entry; first = True
    def rn(r):
        return {"eax": "rax", "ecx": "rcx", "edx": "rdx", "r8d": "r8", "r9d": "r9", "r10d": "r10", "r11d": "r11"}.get(r, r)
    for step in range(20000):
        if pc == EXIT:
            return "exit"
        i = ins_at(pc); m, ops = i.mnemonic, i.operands; nxt = pc + i.size
        if m == "mov" and ops[0].type == X86_OP_REG:
            d = rn(i.reg_name(ops[0].reg))
            if ops[1].type == X86_OP_IMM:
                R[d] = ops[1].imm & 0xFFFFFFFF
            elif ops[1].type == X86_OP_MEM and first:
                R[d] = ret; first = False
            elif ops[1].type == X86_OP_REG:
                R[d] = R.get(rn(i.reg_name(ops[1].reg)), 0)
        elif m == "lea" and ops[1].type == X86_OP_MEM and ops[1].mem.base == X86_REG_RIP:
            R[rn(i.reg_name(ops[0].reg))] = BASE + nxt + ops[1].mem.disp
        elif m == "cmp":
            if ops[0].type == X86_OP_MEM:
                v = retbytes.get(ops[0].mem.disp, 0); b = ops[1].imm & 0xFF
            else:
                v = R.get(rn(i.reg_name(ops[0].reg)), 0)
                b = ops[1].imm & 0xFFFFFFFF if ops[1].type == X86_OP_IMM else R.get(rn(i.reg_name(ops[1].reg)), 0)
                if i.reg_name(ops[0].reg).startswith("e"):
                    v &= 0xFFFFFFFF
            fl = {"z": v == b, "a": v > b, "b": v < b}
        elif m == "xor" and ops[0].type == X86_OP_REG and ops[1].type == X86_OP_REG:
            a_, b_ = rn(i.reg_name(ops[0].reg)), rn(i.reg_name(ops[1].reg))
            R[a_] = R.get(a_, 0) ^ R.get(b_, 0)
        elif m.startswith("cmov"):
            c = {"cmova": fl["a"], "cmovae": not fl["b"], "cmovb": fl["b"], "cmovbe": not fl["a"],
                 "cmove": fl["z"], "cmovne": not fl["z"]}[m]
            if c:
                R[rn(i.reg_name(ops[0].reg))] = R.get(rn(i.reg_name(ops[1].reg)), 0)
        elif m.startswith("j"):
            c = {"jmp": True, "je": fl.get("z"), "jne": not fl.get("z"), "ja": fl.get("a"),
                 "jb": fl.get("b"), "jae": not fl.get("b"), "jbe": not fl.get("a")}.get(m)
            if c is None:
                return "?" + m
            if c:
                if ops[0].type != X86_OP_IMM:
                    return "?indirect"
                nxt = ops[0].imm
        elif m in ("ret", "call", "int3"):
            return "?" + m
        pc = nxt
    return "HANG"
for fs in ARGS:
    f = int(fs, 16)
    s, EXIT, tbl = guard(f)
    if tbl is None or EXIT is None:
        print("%08X guard@%08X: no jump table / exit found" % (f, s)); continue
    res = []
    for k in range(15):
        c = struct.unpack_from("<I", IMG, tbl + 4 * k)[0]
        res.append((run(c, 0x7FF76C349C53, {-2: 0xFF, -5: 0xE8}, EXIT), run(c, BASE - 0x100000, {-2: 0xFF, -5: 0xE8}, EXIT),
                    run(c, BASE + 0x100000, {-2: 0xFF, -5: 0xE8}, EXIT)))
    ab = [k for k, r in enumerate(res) if r[0] != "exit"]
    be = [k for k, r in enumerate(res) if r[1] != "exit"]
    ok = [k for k, r in enumerate(res) if r[2] != "exit"]
    print("%08X guard@%08X exit %08X  caller ABOVE image hangs cases %s | BELOW %s | in-image(sanity) %s" % (
        f, s, EXIT or 0, ab, be, ok))
