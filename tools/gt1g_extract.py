#!/usr/bin/env python3
"""Strict GT1G 0600 texture extractor for LINK containers (res/res_grp.bin etc.).

Usage:
  gt1g_extract.py FILE --list [-v]
  gt1g_extract.py FILE ENTRY [TEXTURE] --out DIR [--flip]
  gt1g_extract.py FILE ENTRY --record N --out DIR [--scale K]
  gt1g_extract.py FILE --all --out DIR

Layout (see docs/formats/link.md and gt1g.md):
  top level: 'LINK', count, 1, 0; entry table (offset, size) at 0x10
  nested:    'LINK', 1, 0x20, n, 0x40; (data_off, data_size) at +0x20;
             n descriptor records (u16 x, y, w, h; u32 texture index) at +0x40
  GT1G:      'GT1G0600', total, header_size, count, 0, 0, count x u32, offset table,
             then per texture a 20-byte entry header followed by pixel payload.
  entry header byte 1 = format, byte 2 = size (low nibble log2 width, high nibble
  log2 height), u32 at +8 = 0x0C.

Only format 0x01 (4 bytes per pixel) is decoded. Other formats are reported as
unsupported. Pixel channel order is not decided: both BGRA and RGBA are written.
Never commit the output; write it outside the repository with --out.
"""
import argparse
import os
import struct
import sys
from collections import Counter
from dataclasses import dataclass

BPP = {0x01: 4}
TEX_ENTRY_HEADER = 20
MAX_TEXTURES = 64
MAX_RECORDS = 100000


class Gt1gError(Exception):
    pass


class NotNestedLink(Gt1gError):
    pass


@dataclass
class Texture:
    index: int
    fmt: int
    width: int
    height: int
    pixel_offset: int
    payload_size: int

    @property
    def supported(self):
        return self.fmt in BPP

    @property
    def bits_per_pixel(self):
        return self.payload_size * 8 / (self.width * self.height)


@dataclass
class Nested:
    base: int
    size: int
    n: int
    desc_off: int
    data_off: int
    data_size: int


@dataclass
class Gt1g:
    start: int
    total: int
    header_size: int
    textures: list


def read_exact(f, offset, size, what):
    f.seek(offset)
    data = f.read(size)
    if len(data) != size:
        raise Gt1gError(f'{what}: truncated at 0x{offset:x} (wanted {size}, got {len(data)})')
    return data


def read_top_level(f, file_size):
    head = read_exact(f, 0, 16, 'top header')
    if head[:4] != b'LINK':
        raise Gt1gError('not a LINK file')
    count = struct.unpack_from('<I', head, 4)[0]
    if count == 0 or 16 + 8 * count > file_size:
        raise Gt1gError(f'bad entry count {count}')
    raw = read_exact(f, 16, 8 * count, 'top table')
    entries = []
    for i in range(count):
        off, size = struct.unpack_from('<II', raw, 8 * i)
        if size and off + size > file_size:
            raise Gt1gError(f'entry {i}: payload 0x{off:x}+0x{size:x} is beyond the end of the file')
        entries.append((off, size))
    return entries


def read_nested(f, index, base, size):
    if size < 0x40:
        raise NotNestedLink(f'entry {index}: payload too small ({size} bytes)')
    head = read_exact(f, base, 0x28, f'entry {index} nested header')
    if head[:4] != b'LINK':
        raise NotNestedLink(f'entry {index}: first bytes {head[:4]!r}, not LINK')
    ver, table_off, n, desc_off = struct.unpack_from('<4I', head, 4)
    if (ver, table_off, desc_off) != (1, 0x20, 0x40):
        raise NotNestedLink(
            f'entry {index}: LINK with unsupported layout (v={ver}, table=0x{table_off:x}, desc=0x{desc_off:x})')
    data_off, data_size = struct.unpack_from('<II', head, 0x20)
    if n == 0 or n > MAX_RECORDS:
        raise Gt1gError(f'entry {index}: bad record count {n}')
    if data_off < desc_off + 12 * n:
        raise Gt1gError(f'entry {index}: data starts at 0x{data_off:x}, inside the descriptor table')
    if data_off + data_size != size:
        raise Gt1gError(
            f'entry {index}: data 0x{data_off:x}+0x{data_size:x} does not end at payload size 0x{size:x}')
    return Nested(base, size, n, desc_off, data_off, data_size)


def read_descriptors(f, nested):
    raw = read_exact(f, nested.base + nested.desc_off, 12 * nested.n, 'descriptor table')
    return [struct.unpack_from('<HHHHI', raw, 12 * i) for i in range(nested.n)]


def read_gt1g(f, nested):
    start = nested.base + nested.data_off
    head = read_exact(f, start, 0x1C, 'GT1G header')
    if head[:8] != b'GT1G0600':
        raise Gt1gError(f'bad GT1G tag {head[:8]!r}')
    total, hsize, count = struct.unpack_from('<3I', head, 8)
    f14, f18 = struct.unpack_from('<2I', head, 0x14)
    if total != nested.data_size:
        raise Gt1gError(f'GT1G total 0x{total:x} != nested data size 0x{nested.data_size:x}')
    if count < 1 or count > MAX_TEXTURES:
        raise Gt1gError(f'bad texture count {count}')
    if hsize != 0x1C + 4 * count:
        raise Gt1gError(f'GT1G header_size 0x{hsize:x} != 0x{0x1C + 4 * count:x}')
    if f14 != 0 or f18 != 0:
        raise Gt1gError(f'unexpected header fields {f14}, {f18}')
    raw = read_exact(f, start + hsize, 4 * count, 'texture offset table')
    offs = list(struct.unpack('<%dI' % count, raw))
    if offs[0] != 4 * count:
        raise Gt1gError(f'first texture offset 0x{offs[0]:x} != 0x{4 * count:x}')
    body = total - hsize
    ends = offs[1:] + [body]
    textures = []
    for i, (o, e) in enumerate(zip(offs, ends)):
        if not o < e <= body:
            raise Gt1gError(f'texture {i}: bad range 0x{o:x}..0x{e:x} (body 0x{body:x})')
        span = e - o
        if span < TEX_ENTRY_HEADER:
            raise Gt1gError(f'texture {i}: span {span} smaller than its header')
        entry = start + hsize + o
        t = read_exact(f, entry, TEX_ENTRY_HEADER, f'texture {i} header')
        ext = struct.unpack_from('<I', t, 8)[0]
        if ext != 0x0C:
            raise Gt1gError(f'texture {i}: extra header size 0x{ext:x} != 0x0c')
        fmt = t[1]
        width = 1 << (t[2] & 0xF)
        height = 1 << (t[2] >> 4)
        payload = span - TEX_ENTRY_HEADER
        if fmt in BPP and payload != width * height * BPP[fmt]:
            raise Gt1gError(
                f'texture {i}: payload {payload} != {width}x{height}x{BPP[fmt]} for format 0x{fmt:02x}')
        textures.append(Texture(i, fmt, width, height, entry + TEX_ENTRY_HEADER, payload))
    return Gt1g(start, total, hsize, textures)


def load_entry(f, index, base, size):
    nested = read_nested(f, index, base, size)
    return nested, read_gt1g(f, nested)


def descriptor_issues(textures, records):
    issues = []
    for i, (x, y, w, h, last) in enumerate(records):
        if last >= len(textures):
            issues.append((i, f'texture index {last} out of range'))
            continue
        t = textures[last]
        if x + w > t.width or y + h > t.height:
            issues.append((i, f'rect {x},{y},{w},{h} exceeds texture {last} ({t.width}x{t.height})'))
    return issues


def decode_image(data, width, height, order):
    from PIL import Image
    return Image.frombytes('RGBA', (width, height), data, 'raw', order)


def save_variants(data, tex, out_dir, name, crop, scale, flip):
    from PIL import Image
    written = 0
    for order in ('BGRA', 'RGBA'):
        img = decode_image(data, tex.width, tex.height, order)
        if crop:
            img = img.crop(crop)
        if scale > 1:
            img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
        variants = [('', img)]
        if flip:
            variants.append(('_flip', img.transpose(Image.FLIP_TOP_BOTTOM)))
        for suffix, im in variants:
            im.save(os.path.join(out_dir, f'{name}_{order.lower()}{suffix}.png'))
            written += 1
    return written


def cmd_list(f, entries, verbose):
    for idx, (base, size) in enumerate(entries):
        try:
            nested, gt = load_entry(f, idx, base, size)
            records = read_descriptors(f, nested)
        except NotNestedLink as exc:
            print(f'[{idx}] skip: {exc}')
            continue
        except Gt1gError as exc:
            print(f'[{idx}] INVALID: {exc}')
            continue
        fmts = Counter(t.fmt for t in gt.textures)
        fmt_s = ', '.join(f'0x{k:02x}:{v}' for k, v in sorted(fmts.items()))
        issues = descriptor_issues(gt.textures, records)
        print(f'[{idx}] textures={len(gt.textures)} records={len(records)} formats=[{fmt_s}] descriptor_issues={len(issues)}')
        if verbose:
            for t in gt.textures:
                print(f'    tex {t.index}: fmt=0x{t.fmt:02x} {t.width}x{t.height} payload={t.payload_size} bpp={t.bits_per_pixel:g}')
            for i, why in issues[:10]:
                print(f'    record {i}: {why}')
    return 0


def cmd_extract(f, entries, args):
    if not args.out:
        print('--out DIR is required for extraction', file=sys.stderr)
        return 2
    os.makedirs(args.out, exist_ok=True)
    explicit = args.entry is not None
    indices = [args.entry] if explicit else range(len(entries))
    written = unsupported = skipped = 0
    status = 0
    for idx in indices:
        base, size = entries[idx]
        try:
            nested, gt = load_entry(f, idx, base, size)
            records = read_descriptors(f, nested)
        except Gt1gError as exc:
            print(f'[{idx}] skipped: {exc}', file=sys.stderr)
            skipped += 1
            status = 1 if explicit else status
            continue
        picks, crop, label = list(gt.textures), None, None
        if args.record is not None:
            if not 0 <= args.record < len(records):
                print(f'[{idx}] record {args.record} out of range (0-{len(records) - 1})', file=sys.stderr)
                return 1
            x, y, w, h, last = records[args.record]
            if last >= len(gt.textures):
                print(f'[{idx}] record {args.record}: texture index {last} out of range', file=sys.stderr)
                return 1
            picks, crop, label = [gt.textures[last]], (x, y, x + w, y + h), f'r{args.record}'
        elif args.texture is not None:
            if not 0 <= args.texture < len(gt.textures):
                print(f'[{idx}] texture {args.texture} out of range (0-{len(gt.textures) - 1})', file=sys.stderr)
                return 1
            picks = [gt.textures[args.texture]]
        for tex in picks:
            if not tex.supported:
                print(f'[{idx}] texture {tex.index}: unsupported format 0x{tex.fmt:02x} '
                      f'({tex.width}x{tex.height}, {tex.bits_per_pixel:g} bits/pixel), not written')
                unsupported += 1
                if args.texture is not None or args.record is not None:
                    status = 3
                continue
            if crop and (crop[2] > tex.width or crop[3] > tex.height):
                print(f'[{idx}] crop {crop} exceeds texture {tex.index} ({tex.width}x{tex.height})', file=sys.stderr)
                status = 1
                continue
            data = read_exact(f, tex.pixel_offset, tex.payload_size, f'texture {tex.index} pixels')
            name = f'e{idx}_{label + "_" if label else ""}t{tex.index}_{tex.width}x{tex.height}'
            written += save_variants(data, tex, args.out, name, crop, args.scale, args.flip)
    print(f'written={written} unsupported={unsupported} skipped_entries={skipped}')
    return status


def main(argv=None):
    ap = argparse.ArgumentParser(description='Strict GT1G 0600 texture extractor')
    ap.add_argument('file')
    ap.add_argument('entry', nargs='?', type=int)
    ap.add_argument('texture', nargs='?', type=int)
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--all', action='store_true')
    ap.add_argument('-v', '--verbose', action='store_true')
    ap.add_argument('--out')
    ap.add_argument('--record', type=int)
    ap.add_argument('--scale', type=int, default=1)
    ap.add_argument('--flip', action='store_true')
    args = ap.parse_args(argv)
    try:
        file_size = os.path.getsize(args.file)
        with open(args.file, 'rb') as f:
            entries = read_top_level(f, file_size)
            if args.list or (args.entry is None and not args.all):
                return cmd_list(f, entries, args.verbose)
            if args.entry is not None and not 0 <= args.entry < len(entries):
                print(f'entry {args.entry} out of range (0-{len(entries) - 1})', file=sys.stderr)
                return 1
            return cmd_extract(f, entries, args)
    except Gt1gError as exc:
        print(f'error: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
