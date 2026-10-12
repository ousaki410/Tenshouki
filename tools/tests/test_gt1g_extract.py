import io
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import gt1g_extract as g  # noqa: E402


def build_gt1g(textures):
    count = len(textures)
    hsize = 0x1C + 4 * count
    entries = []
    for fmt, wl, hl, payload in textures:
        head = bytes([0x10, fmt, (hl << 4) | wl, 0, 0x11, 0x11, 0x01, 0x10])
        head += struct.pack('<I', 0x0C) + b'\0' * 7 + b'\x01'
        entries.append(head + payload)
    offs, cur = [], 4 * count
    for e in entries:
        offs.append(cur)
        cur += len(e)
    total = hsize + cur
    out = b'GT1G0600' + struct.pack('<3I', total, hsize, count) + struct.pack('<2I', 0, 0)
    out += struct.pack('<%dI' % count, *([1] * count))
    out += struct.pack('<%dI' % count, *offs) + b''.join(entries)
    assert len(out) == total
    return out


def build_nested(gt, records):
    n = len(records)
    data_off = ((0x40 + 12 * n) & ~0xF) + 0x10
    hdr = bytearray(0x40)
    hdr[0:4] = b'LINK'
    struct.pack_into('<4I', hdr, 4, 1, 0x20, n, 0x40)
    struct.pack_into('<II', hdr, 0x20, data_off, len(gt))
    body = bytes(hdr) + b''.join(struct.pack('<HHHHI', *r) for r in records)
    return body + b'\0' * (data_off - len(body)) + gt


def build_top(payloads):
    count = len(payloads)
    off = 16 + 8 * count
    table, body = b'', b''
    for p in payloads:
        table += struct.pack('<II', off, len(p))
        body += p + b'\xef\xcd\xab\x89'
        off += len(p) + 4
    return b'LINK' + struct.pack('<III', count, 1, 0) + table + body


def parse(blob, index=0):
    f = io.BytesIO(blob)
    entries = g.read_top_level(f, len(blob))
    nested, gt = g.load_entry(f, index, *entries[index])
    return f, entries, nested, gt


def make_blob(textures, records=((0, 0, 1, 1, 0),)):
    return build_top([build_nested(build_gt1g(textures), list(records))])


class Gt1gExtractTests(unittest.TestCase):
    def test_top_level_table_starts_at_0x10(self):
        a = build_nested(build_gt1g([(1, 0, 0, bytes(4))]), [(0, 0, 1, 1, 0)])
        blob = build_top([a, a])
        entries = g.read_top_level(io.BytesIO(blob), len(blob))
        self.assertEqual(entries[0], (16 + 16, len(a)))
        self.assertEqual(entries[1][0], 16 + 16 + len(a) + 4)

    def test_width_is_low_nibble_and_pixels_start_after_header(self):
        p0, p1 = bytes(range(32)), bytes(range(100, 132))
        f, _, _, gt = parse(make_blob([(1, 1, 2, p0), (1, 2, 1, p1)]))
        t0, t1 = gt.textures
        self.assertEqual((t0.width, t0.height), (2, 4))
        self.assertEqual((t1.width, t1.height), (4, 2))
        self.assertEqual(g.read_exact(f, t0.pixel_offset, t0.payload_size, 'p'), p0)
        self.assertEqual(g.read_exact(f, t1.pixel_offset, t1.payload_size, 'p'), p1)

    def test_unsupported_format_is_reported_not_rejected(self):
        _, _, _, gt = parse(make_blob([(0x08, 3, 2, bytes(32))]))
        t = gt.textures[0]
        self.assertFalse(t.supported)
        self.assertEqual((t.width, t.height), (8, 4))
        self.assertEqual(t.bits_per_pixel, 8)

    def test_span_mismatch_is_rejected(self):
        with self.assertRaises(g.Gt1gError):
            parse(make_blob([(1, 1, 1, bytes(12))]))

    def test_total_mismatch_is_rejected(self):
        gt = build_gt1g([(1, 0, 0, bytes(4))])
        gt = gt[:8] + struct.pack('<I', len(gt) + 4) + gt[12:]
        blob = build_top([build_nested(gt, [(0, 0, 1, 1, 0)])])
        with self.assertRaises(g.Gt1gError):
            parse(blob)

    def test_bad_version_is_rejected(self):
        blob = make_blob([(1, 0, 0, bytes(4))]).replace(b'GT1G0600', b'GT1G0500')
        with self.assertRaises(g.Gt1gError):
            parse(blob)

    def test_non_link_payload_is_skipped_as_not_nested(self):
        blob = build_top([b'_N1G' + bytes(0x80)])
        with self.assertRaises(g.NotNestedLink):
            parse(blob)

    def test_top_level_overflow_is_rejected(self):
        blob = bytearray(make_blob([(1, 0, 0, bytes(4))]))
        struct.pack_into('<II', blob, 16, 0x30, 10 ** 9)
        with self.assertRaises(g.Gt1gError):
            g.read_top_level(io.BytesIO(bytes(blob)), len(blob))

    def test_descriptor_issues(self):
        records = [(0, 0, 4, 2, 0), (0, 0, 5, 2, 0), (0, 0, 1, 1, 3)]
        _, _, _, gt = parse(make_blob([(1, 2, 1, bytes(32))], records))
        issues = g.descriptor_issues(gt.textures, records)
        self.assertEqual([i for i, _ in issues], [1, 2])

    def test_channel_order(self):
        try:
            import PIL  # noqa: F401
        except ImportError:
            self.skipTest('Pillow not installed')
        data = bytes([1, 2, 3, 255])
        self.assertEqual(g.decode_image(data, 1, 1, 'BGRA').getpixel((0, 0)), (3, 2, 1, 255))
        self.assertEqual(g.decode_image(data, 1, 1, 'RGBA').getpixel((0, 0)), (1, 2, 3, 255))


if __name__ == '__main__':
    unittest.main()
