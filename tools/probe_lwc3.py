import struct, sys, pathlib
from collections import Counter
def u32(b, o): return struct.unpack_from("<I", b, o)[0]
def entries(d, base, length):
    if length < 32 or d[base:base+4] != b"LINK": return None
    c = u32(d, base+4)
    t = 32 if u32(d, base+8) == 32 else 16
    if c > 200000 or t + 8*c > length: return None
    return [struct.unpack_from("<II", d, base+t+8*i) for i in range(c)]
d = pathlib.Path(sys.argv[1]).read_bytes()
for i, (o, s) in enumerate(entries(d, 0, len(d))):
    es = entries(d, o, s)
    if not es or u32(d, o + 8) != 32: continue
    n = perm = h268 = 0; wants = Counter(); odd = []; ratio = []
    for lo, ls in es:
        if ls < 268 or d[o+lo:o+lo+4] != b"LWC\x1a": continue
        b = d[o+lo:o+lo+ls]; n += 1
        want, comp = u32(b, 4), u32(b, 8)
        h268 += (ls - comp == 268); wants[want] += 1
        if sorted(b[12:268]) == list(range(256)): perm += 1
        elif len(odd) < 3: odd.append(b[12:28].hex())
        if comp: ratio.append(want / comp)
    if n:
        print("L1", i, "leaves", n, "hdr268", h268, "perm", perm, "top_want", wants.most_common(3),
              "ratio min/max", round(min(ratio), 2), round(max(ratio), 2), "nonperm_head", odd)
