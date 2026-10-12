import struct, sys, zlib, pathlib
def u32(b, o): return struct.unpack_from("<I", b, o)[0]
def entries(d, base, length):
    if length < 32 or d[base:base+4] != b"LINK": return None
    c = u32(d, base+4)
    t = 32 if u32(d, base+8) == 32 else 16
    if c > 200000 or t + 8*c > length: return None
    return [struct.unpack_from("<II", d, base+t+8*i) for i in range(c)]
def write_png(path, w, h, px):
    raw = bytearray()
    for y in range(h):
        raw.append(0); raw += px[y*w*4:(y+1)*w*4]
    def ch(t, data):
        return struct.pack(">I", len(data)) + t + data + struct.pack(">I", zlib.crc32(t + data) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
                + ch(b"IDAT", zlib.compress(bytes(raw), 6)) + ch(b"IEND", b""))
def c565(v):
    return ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
def dxt(data, w, h, kind):
    out = bytearray(w * h * 4); bs = 8 if kind == 1 else 16; pos = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            blk = data[pos:pos + bs]; pos += bs
            cb = blk if kind == 1 else blk[8:]
            c0, c1, bits = struct.unpack_from("<HHI", cb)
            p0, p1 = c565(c0), c565(c1)
            if c0 > c1 or kind == 5:
                pal = [p0 + (255,), p1 + (255,),
                       tuple((2 * a + b) // 3 for a, b in zip(p0, p1)) + (255,),
                       tuple((a + 2 * b) // 3 for a, b in zip(p0, p1)) + (255,)]
            else:
                pal = [p0 + (255,), p1 + (255,), tuple((a + b) // 2 for a, b in zip(p0, p1)) + (255,), (0, 0, 0, 0)]
            if kind == 5:
                a0, a1 = blk[0], blk[1]
                ab = int.from_bytes(blk[2:8], "little")
                if a0 > a1: ap = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
                else: ap = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
            for i in range(16):
                px = bx + (i & 3); py = by + (i >> 2)
                r, g, b, a = pal[(bits >> (2 * i)) & 3]
                if kind == 5: a = ap[(ab >> (3 * i)) & 7]
                o = (py * w + px) * 4
                out[o:o + 4] = bytes((r, g, b, a))
    return bytes(out)
def decode(fmt, data, w, h, rgba):
    if fmt == 1:
        o = bytearray(data[:w * h * 4])
        if not rgba: o[0::4], o[2::4] = o[2::4], o[0::4]
        return bytes(o)
    if fmt == 6: return dxt(data, w, h, 1)
    if fmt == 8: return dxt(data, w, h, 5)
    return None
def need(fmt, w, h):
    return {1: w * h * 4, 6: w * h // 2, 8: w * h}.get(fmt)
def convert(i, j, p):
    hdr, cnt = u32(p, 12), u32(p, 16)
    offs = [hdr + u32(p, hdr + 4 * k) for k in range(cnt)] + [len(p)]
    for k in range(cnt):
        ts = offs[k]; b0, fmt, dim = p[ts], p[ts + 1], p[ts + 2]
        w, h = 1 << (dim & 15), 1 << (dim >> 4)
        flags = u32(p, ts + 4)
        ds = ts + 8 + (u32(p, ts + 8) if flags & 1 else 0)
        n = need(fmt, w, h); span = offs[k + 1] - ds
        tag = "c%02d_e%d_t%02d_%dx%d_f%02x" % (i, j, k, w, h, fmt)
        if n is None: print(tag, "mips", b0 >> 4, "UNSUPPORTED fmt"); continue
        if span < n: print(tag, "SIZE MISMATCH span", span, "need", n); continue
        if w * h > (1 << 22): print(tag, "skip big"); continue
        px = decode(fmt, p[ds:ds + n], w, h, rgba)
        write_png(out / (tag + ".png"), w, h, px)
        print(tag, "mips", b0 >> 4, "ok span", span, "need", n)
args = [a for a in sys.argv[1:] if not a.startswith("--")]
rgba = "--rgba" in sys.argv
src = pathlib.Path(args[0]); out = pathlib.Path(args[1] if len(args) > 1 else "extracted/tex")
out.mkdir(parents=True, exist_ok=True)
d = src.read_bytes(); top = entries(d, 0, len(d))
for i, (o, s) in enumerate(top):
    kids = entries(d, o, s)
    if not kids or u32(d, o + 8) != 32: continue
    for j, (off, sz) in enumerate(kids):
        if sz < 32 or off + sz > s or d[o + off:o + off + 4] != b"G1TG": continue
        convert(i, j, d[o + off:o + off + sz])
