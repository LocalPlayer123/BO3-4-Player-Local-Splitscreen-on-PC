"""Which game functions does the component call from its own DLL, and which
of them check their caller's return address (Arxan caller guard)?

    python tools/caller_guard_audit.py [--image <dump>]

A guarded function called from outside the game image (the mod's DLL, a
host client's hook) can spin forever in its state machine: ClientCommand
under ezz (2026-09-29, EZZ_REQUIRED_CHANGES item 12) and Dvar_SetInt from
the mod (user freeze 2026-09-29 17:47). A guard reads the return address and
compares it with `lea reg, [rip+d]` targets at image+0x20000000 and/or the
image base.

Call targets are collected from the component sources:
  reinterpret_cast<...(*)(...)>(base() + NAME | b + NAME | base() + 0x...)
  NAME_hook.create(base() + NAME ...) / create(b + 0x...)
where NAME is a `constexpr ... NAME = 0x...;` in the same sources. A function
is scanned over its .pdata extent plus 0x3000 bytes of following chunks up to
the next .pdata function start (Arxan splits bodies into chunks).
"""
import bisect, os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
args = sys.argv[1:]
IMAGE = os.path.join(ROOT, "BlackOps3_06531394_UNPACKED.exe")
if "--image" in args:
    IMAGE = args[args.index("--image") + 1]
img = open(IMAGE, "rb").read()
comp = os.path.join(ROOT, "component")
if not os.path.isdir(comp):
    comp = os.path.join(ROOT, "src", "component")   # layout of the public repository
srcs = [os.path.join(comp, f) for f in ("splitscreen.cpp", "splitscreen_ezz.hpp", "splitscreen_addresses.hpp")]
parts = os.path.join(comp, "splitscreen")
if os.path.isdir(parts):
    srcs += [os.path.join(parts, f) for f in sorted(os.listdir(parts)) if f.endswith(".inl")]
text = "\n".join(open(p, encoding="utf-8").read() for p in srcs)

consts = {m.group(1): int(m.group(2), 16) for m in re.finditer(r"constexpr\s+(?:uint32_t|size_t|uintptr_t|auto)\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+)", text)}
targets = {}
for m in re.finditer(r"reinterpret_cast<[^;]*?\(\s*\*\s*\)\s*\([^;]*?>\s*\(\s*(?:base\(\)|b)\s*\+\s*(\w+)\s*\)", text):
    targets.setdefault(m.group(1), "call")
# the same through a function-pointer alias: `using fn_t = R (*)(...);` ... reinterpret_cast<fn_t>(base() + NAME)
fn_aliases = set(re.findall(r"using\s+(\w+)\s*=\s*[^;=]*\(\s*\*\s*\)\s*\(", text))
for m in re.finditer(r"reinterpret_cast<\s*(\w+)\s*>\s*\(\s*(?:base\(\)|b)\s*\+\s*(\w+)\s*\)", text):
    if m.group(1) in fn_aliases:
        targets.setdefault(m.group(2), "call")
for m in re.finditer(r"(\w+)_hook\.create\(\s*(?:reinterpret_cast<void\*>\()?\s*(?:base\(\)|b)\s*\+\s*(\w+)", text):
    targets[m.group(2)] = "hook+invoke"
# hook_if_stock(NAME_hook, NAME_rva, prologue, stub)
for m in re.finditer(r"hook_if_stock\(\s*\w+_hook\s*,\s*(\w+)\s*,", text):
    targets[m.group(1)] = "hook+invoke"
# hooks whose target sits in a local first: `const auto x = base() + NAME;` ... `.create(... x`
for m in re.finditer(r"const\s+auto\s+(\w+)\s*=\s*(?:base\(\)|b)\s*\+\s*(\w+)\s*;", text):
    var, name = m.group(1), m.group(2)
    tail = text[m.end():m.end() + 600]
    if re.search(r"_hook\.create\(\s*(?:reinterpret_cast<void\*>\()?\s*%s" % re.escape(var), tail):
        targets[name] = "hook+invoke"
resolved = {}
for name, kind in targets.items():
    v = int(name, 16) if name.startswith("0x") else consts.get(name)
    if v is not None and v < 0x02EFD600:
        resolved[v] = (name, kind)

pe = struct.unpack_from("<I", img, 0x3C)[0]
pr, ps = struct.unpack_from("<II", img, pe + 24 + 112 + 3 * 8)
funcs = sorted(struct.unpack_from("<II", img, pr + 12 * k) for k in range(ps // 12))
starts = [b for b, _ in funcs]

def extent(rva):
    # follow a short entry stub (`xor r8d,r8d; jmp body` etc.) to the body, then
    # take that body's own .pdata entry - a fixed window spills into the
    # neighbours' guards (false positives)
    body = rva
    for k in range(0, 16):
        if img[rva + k] == 0xE9:
            t = rva + k + 5 + struct.unpack_from("<i", img, rva + k + 1)[0]
            if 0 <= t - rva < 0x100:
                body = t
            break
        if img[rva + k] in (0xC3, 0xCC):
            break
    i = bisect.bisect_right(starts, body) - 1
    if i >= 0 and funcs[i][0] <= body < funcs[i][1]:
        return funcs[i]
    return body, decoded_end(body)


def decoded_end(start, limit=0x400):
    """End of a function without .pdata (a leaf or a thunk like 0x027C1AB0):
    decode until a ret/jmp with no branch target still pointing past it."""
    from capstone import CS_ARCH_X86, CS_MODE_64, Cs
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    furthest = start
    for insn in md.disasm(bytes(img[start:start + limit]), start):
        end = insn.address + insn.size
        if insn.mnemonic.startswith("j") and insn.op_str.startswith("0x"):
            t = int(insn.op_str, 16)
            if start <= t < start + limit:
                furthest = max(furthest, t)
        if insn.mnemonic in ("ret", "jmp") and end > furthest:
            return end
    return start + limit

def guarded(b, e):
    """(range tests above, range tests below, call-byte tests) in [b, e).
    Range test: lea reg,[rip+d] to image+0x20000000 / image base.
    Call-byte test: cmp byte ptr [rax-5], 0xE8 / [rax-4|-7], 0xFF - the byte in
    front of the return address must be a call (Dvar getter 0x02261DF2)."""
    above = below = callbyte = 0
    for p in range(b, e - 6):          # 7-byte lea ends at p + 7 <= e
        if img[p] in (0x48, 0x4C) and img[p + 1] == 0x8D and (img[p + 2] & 0xC7) == 0x05:
            t = p + 7 + struct.unpack_from("<i", img, p + 3)[0]
            if t == 0x20000000: above += 1
            elif t == 0: below += 1
        if img[p] == 0x80 and img[p + 1] == 0x78 and (img[p + 2], img[p + 3]) in ((0xFB, 0xE8), (0xFC, 0xFF), (0xF9, 0xFF)):
            callbyte += 1
    return above, below, callbyte


def tail_targets(b, e):
    """Functions the body jumps into (jmp rel32 out of [b, e)): a tail call
    runs the target with THIS function's return address - the caller's."""
    out = set()
    for p in range(b, e - 4):          # E9 + rel32 ends at p + 5 <= e
        if img[p] == 0xE9:
            t = p + 5 + struct.unpack_from("<i", img, p + 1)[0]
            if not b <= t < e and 0x1000 <= t < 0x02EFD600:
                out.add(t)
    return out

SMALL = 0x80   # a wrapper this short that ends in a jmp hands on its caller's return address


def tail_chain(b, e, depth=0):
    """Guarded code reached by jmp from a SMALL function, following jmp chains
    (0x027C1AB0 -> 0x02261D40 -> 0x02261DF2)."""
    found = []
    if e - b > SMALL or depth > 3:
        return found
    for t in sorted(tail_targets(b, e)):
        tb, te = extent(t)
        ta, tbl, tcb = guarded(tb, te)
        if ta or tcb:
            found.append("%08X(range %d, call-byte %d)" % (t, ta, tcb))
        found += tail_chain(tb, te, depth + 1) if (ta == 0 and tcb == 0) else []
        # a jmp into the middle of a chunk: scan the chunk from the target on
        if not (ta or tcb) and te - tb <= SMALL:
            continue
    # jmp straight into code without its own .pdata entry: scan 0x200 bytes there
    for t in sorted(tail_targets(b, e)):
        la, lbl, lcb = guarded(t, t + 0x200)
        if (la or lcb) and not any(s.startswith("%08X" % t) for s in found):
            found.append("%08X+0x200(range %d, call-byte %d)" % (t, la, lcb))
    return found


rows = []
for rva, (name, kind) in sorted(resolved.items()):
    b, e = extent(rva)
    a, bl, cb = guarded(b, e)
    rows.append((rva, name, kind, b, e, a, bl, cb, tail_chain(b, e)))
print("%d call/hook targets resolved from the component sources" % len(rows))
if "--list" in args:
    for rva, name, kind, b, e, a, bl, cb, tails in rows:
        print("  %08X %-38s %-12s above %d  base %d  call-byte %d  tail %s"
              % (rva, name, kind, a, bl, cb, ", ".join(tails) or "none"))
flag = [r for r in rows if r[5] or r[7] or r[8]]
print("%d contain a caller check, or jump from a small body into one:" % len(flag))
for rva, name, kind, b, e, a, bl, cb, tails in flag:
    print("  %08X %-38s %-12s func %08X-%08X  above %d  base %d  call-byte %d  tail %s"
          % (rva, name[:38], kind, b, e, a, bl, cb, ", ".join(tails) or "-"))
print("A hook whose stub calls invoke() on a flagged target, or a direct call of")
print("one, can hang: compute the answer instead, or call from inside the image.")
print("(Large functions jump into Arxan chunks everywhere; those are not followed.)")
