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
srcs = [os.path.join(comp, f) for f in ("splitscreen.cpp", "splitscreen_ezz.hpp", "splitscreen_signin.hpp")]
parts = os.path.join(comp, "splitscreen")
if os.path.isdir(parts):
    srcs += [os.path.join(parts, f) for f in sorted(os.listdir(parts)) if f.endswith(".inl")]
text = "\n".join(open(p, encoding="utf-8").read() for p in srcs)

consts = {m.group(1): int(m.group(2), 16) for m in re.finditer(r"constexpr\s+(?:uint32_t|size_t|uintptr_t|auto)\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+)", text)}
targets = {}
for m in re.finditer(r"reinterpret_cast<[^;]*?\(\s*\*\s*\)\s*\([^;]*?>\s*\(\s*(?:base\(\)|b)\s*\+\s*(\w+)\s*\)", text):
    targets.setdefault(m.group(1), "call")
for m in re.finditer(r"(\w+)_hook\.create\(\s*(?:reinterpret_cast<void\*>\()?\s*(?:base\(\)|b)\s*\+\s*(\w+)", text):
    targets[m.group(2)] = "hook+invoke"
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
    return body, body + 0x200

def guarded(b, e):
    above = below = 0
    for p in range(b, e - 7):
        if img[p] in (0x48, 0x4C) and img[p + 1] == 0x8D and (img[p + 2] & 0xC7) == 0x05:
            t = p + 7 + struct.unpack_from("<i", img, p + 3)[0]
            if t == 0x20000000: above += 1
            elif t == 0: below += 1
    return above, below

rows = []
for rva, (name, kind) in sorted(resolved.items()):
    b, e = extent(rva)
    a, bl = guarded(b, e)
    rows.append((rva, name, kind, b, e, a, bl))
print("%d call/hook targets resolved from the component sources" % len(rows))
flag = [r for r in rows if r[5]]
print("%d contain a caller range test (lea -> image+0x20000000):" % len(flag))
for rva, name, kind, b, e, a, bl in flag:
    print("  %08X %-38s %-12s func %08X-%08X  above-tests %d  base-tests %d" % (rva, name[:38], kind, b, e, a, bl))
