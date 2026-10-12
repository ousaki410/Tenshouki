import struct, sys, zlib, bz2, lzma, pathlib
def u32(b, o): return struct.unpack_from("<I", b, o)[0]
def entries(d, base, length):
    if length < 32 or d[base:base+4] != b"LINK": return None
    c = u32(d, base+4)
    t = 32 if u32(d, base+8) == 32 else 16
    if c > 200000 or t + 8*c > length: return None
    return [struct.unpack_from("<II", d, base+t+8*i) for i in range(c)]
d = pathlib.Path(sys.argv[1]).read_bytes(); idx = int(sys.argv[2])
top = entries(d, 0, len(d)); o, s = top[idx]; es = entries(d, o, s)
rc, ro = u32(d, o + 12), u32(d, o + 16)
print("records", rc, [struct.unpack_from("<III", d, o + ro + 12 * i) for i in range(min(rc, 6))])
leaves = sorted((ls, lo) for lo, ls in es if ls > 300 and d[o+lo:o+lo+4] == b"LWC\x1a")[:4]
for ls, lo in leaves:
    b = d[o+lo:o+lo+ls]; want, comp = u32(b, 4), u32(b, 8); blk = b[12:268]
    print("leaf", ls, "want", want, "comp", comp, "hdr", ls - comp, "u8[12:28]", b[12:28].hex())
    print("  block256 distinct", len(set(blk)), "perm", sorted(blk) == list(range(256)), "zeros", blk.count(0))
    hit = False
    for off in range(0, 400):
        for wb in (15, -15, 31):
            try: x = zlib.decompressobj(wb).decompress(b[off:])
            except Exception: continue
            if x: print("  zlib off", off, "wbits", wb, "out", len(x), "match" if len(x) == want else "", x[:4].hex()); hit = True
    for name, fn in (("bz2", bz2.decompress), ("lzma", lzma.decompress)):
        for off in (0, 12, 268):
            try: x = fn(b[off:]); print("  ", name, "off", off, "out", len(x)); hit = True
            except Exception: pass
    if not hit: print("  no zlib/bz2/lzma candidate")
