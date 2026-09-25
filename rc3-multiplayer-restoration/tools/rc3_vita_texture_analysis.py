#!/usr/bin/env python3
"""Analyze paired RC3 PS3/Vita texture tables and compressed VRAM payloads."""

from __future__ import annotations

import argparse
import collections
import struct
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Ps3Texture:
    pointer: int
    mip_count: int
    fmt: int
    width: int
    height: int
    raw: bytes


@dataclass
class VitaTexture:
    pointer: int
    fmt: int
    mip_count: int
    width: int
    height: int
    size: int
    flags: int
    raw: bytes


def u32be(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def u32le(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def mip_layout(width: int, height: int, count: int, block_size: int):
    offset = 0
    for mip in range(count):
        w = max(1, width >> mip)
        h = max(1, height >> mip)
        bw = max(1, (w + 3) // 4)
        bh = max(1, (h + 3) // 4)
        size = bw * bh * block_size
        yield mip, w, h, bw, bh, offset, size
        offset += size


def morton_index(index: int, width: int, height: int) -> int:
    original_width = width
    x_multiplier = y_multiplier = 1
    x_value = y_value = 0
    while width > 1 or height > 1:
        # Vita's compressed-texture Morton order consumes the Y bit first.
        if height > 1:
            y_value += y_multiplier * (index & 1)
            index >>= 1
            y_multiplier *= 2
            height >>= 1
        if width > 1:
            x_value += x_multiplier * (index & 1)
            index >>= 1
            x_multiplier *= 2
            width >>= 1
    return y_value * original_width + x_value


def swizzle_blocks(linear: bytes, block_width: int, block_height: int, block_size: int) -> bytes:
    """Inverse of the Vita unswizzle used by the existing RC1 extractor."""
    output = bytearray(len(linear))
    for source in range(block_width * block_height):
        linear_index = morton_index(source, block_width, block_height)
        output[source * block_size:(source + 1) * block_size] = linear[
            linear_index * block_size:(linear_index + 1) * block_size
        ]
    return bytes(output)


def bc3_alpha_values(block: bytes) -> list[int]:
    a0, a1 = block[0], block[1]
    bits = int.from_bytes(block[2:8], "little")
    table = [a0, a1]
    if a0 > a1:
        table.extend(((7 - i) * a0 + i * a1) // 7 for i in range(1, 7))
    else:
        table.extend(((5 - i) * a0 + i * a1) // 5 for i in range(1, 5))
        table.extend((0, 255))
    return [table[(bits >> (3 * pixel)) & 7] for pixel in range(16)]


def parse(ps3_engine: bytes, ps3_vram: bytes, vita_engine: bytes, vita_vram: bytes):
    ps3_table, ps3_count = u32be(ps3_engine, 0x54), u32be(ps3_engine, 0x58)
    vita_table, vita_count = u32le(vita_engine, 0x54), u32le(vita_engine, 0x58)
    if ps3_count != vita_count:
        raise ValueError(f"texture count mismatch: PS3 {ps3_count}, Vita {vita_count}")
    ps3, vita = [], []
    for index in range(ps3_count):
        p = ps3_table + index * 0x24
        vp = vita_table + index * 0x74
        ps3.append(Ps3Texture(
            u32be(ps3_engine, p), ps3_engine[p + 5], ps3_engine[p + 6],
            struct.unpack_from(">H", ps3_engine, p + 0x18)[0],
            struct.unpack_from(">H", ps3_engine, p + 0x1A)[0],
            ps3_engine[p:p + 0x24],
        ))
        vita.append(VitaTexture(
            u32le(vita_engine, vp), vita_engine[vp + 7], u32le(vita_engine, vp + 0x0C),
            struct.unpack_from("<H", vita_engine, vp + 0x18)[0],
            struct.unpack_from("<H", vita_engine, vp + 0x1A)[0],
            u32le(vita_engine, vp + 0x1C), u32le(vita_engine, vp + 0x20),
            vita_engine[vp:vp + 0x74],
        ))
    return ps3, vita


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("ps3_engine", type=Path)
    ap.add_argument("ps3_vram", type=Path)
    ap.add_argument("vita_engine", type=Path)
    ap.add_argument("vita_vram", type=Path)
    ap.add_argument("--show-failures", type=int, default=20)
    args = ap.parse_args()
    pe, pv, ve, vv = (path.read_bytes() for path in
                       (args.ps3_engine, args.ps3_vram, args.vita_engine, args.vita_vram))
    ps3, vita = parse(pe, pv, ve, vv)

    counts = collections.Counter()
    failures = []
    flag_counts = collections.Counter()
    alpha_shape_counts = collections.Counter()
    flag_matches_alpha_range = 0
    flag_matches_base_alpha_range = 0
    alpha_payload_exact = 0
    swapped = []
    for index, (p, v) in enumerate(zip(ps3, vita)):
        block_size = 8 if v.fmt == 0x85 else 16
        expected = sum(x[-1] for x in mip_layout(v.width, v.height, v.mip_count, block_size))
        expected_aligned = (expected + 15) & ~15
        counts["size_exact" if v.size == expected else "size_aligned" if v.size == expected_aligned else "size_bad"] += 1
        flag_counts[(v.fmt, v.flags)] += 1
        if (p.width, p.height) == (v.height, v.width) and p.width != p.height:
            swapped.append(index)
        elif (p.width, p.height) != (v.width, v.height):
            counts["dimension_other"] += 1

        generated = bytearray()
        alpha_values = set()
        base_alpha_values = set()
        generated_alpha = bytearray()
        for mip, _w, _h, ps_bw, ps_bh, ps_off, ps_size in mip_layout(p.width, p.height, p.mip_count, 16):
            ps_level = pv[p.pointer + ps_off:p.pointer + ps_off + ps_size]
            for off in range(0, len(ps_level), 16):
                values = bc3_alpha_values(ps_level[off:off + 16])
                alpha_values.update(values)
                if mip == 0:
                    base_alpha_values.update(values)
            if block_size == 8:
                linear = b"".join(ps_level[off + 8:off + 16] for off in range(0, len(ps_level), 16))
            else:
                linear = ps_level
            # Vita descriptors transpose every non-square texture, but the block
            # stream retains the PS3 image's original row/column orientation.
            pi = list(mip_layout(p.width, p.height, p.mip_count, block_size))[mip]
            if len(linear) != pi[-1]:
                generated.extend(linear)
            else:
                generated.extend(swizzle_blocks(linear, pi[3], pi[4], block_size))
            if block_size == 16:
                alpha_linear = b"".join(ps_level[off:off + 8] for off in range(0, len(ps_level), 16))
                generated_alpha.extend(swizzle_blocks(alpha_linear, pi[3], pi[4], 8))
        generated.extend(b"\0" * (v.size - len(generated)))
        actual = vv[v.pointer:v.pointer + v.size]
        if block_size == 16:
            actual_alpha = b"".join(actual[off:off + 8] for off in range(0, len(generated_alpha) * 2, 16))
            if bytes(generated_alpha) == actual_alpha:
                alpha_payload_exact += 1
        exact = bytes(generated) == actual
        counts[f"fmt_{v.fmt:02x}"] += 1
        all_opaque = alpha_values == {255}
        binary_alpha = alpha_values <= {0, 255}
        alpha_shape = "opaque" if all_opaque else "binary" if binary_alpha else "graded"
        alpha_shape_counts[(v.fmt, alpha_shape)] += 1
        alpha_min, alpha_max = min(alpha_values), max(alpha_values)
        if (v.flags & 0xFFFF) == ((alpha_max << 8) | alpha_min):
            flag_matches_alpha_range += 1
        base_alpha_min, base_alpha_max = min(base_alpha_values), max(base_alpha_values)
        if (v.flags & 0xFFFF) == ((base_alpha_max << 8) | base_alpha_min):
            flag_matches_base_alpha_range += 1
        counts[f"opaque_{all_opaque}"] += 1
        counts[f"fmt_{v.fmt:02x}_opaque_{all_opaque}"] += 1
        counts["payload_exact" if exact else "payload_mismatch"] += 1
        if not exact and len(failures) < args.show_failures:
            first = next((i for i, (a, b) in enumerate(zip(generated, actual)) if a != b), None)
            failures.append((index, f"{p.width}x{p.height}", f"{v.width}x{v.height}",
                             p.mip_count, v.mip_count, hex(v.fmt), all_opaque, first,
                             len(generated), len(actual)))

    print(f"textures: {len(ps3)}")
    for key in sorted(counts):
        print(f"{key}: {counts[key]}")
    print(f"transposed dimensions: {len(swapped)}; first IDs: {swapped[:40]}")
    print("format/flags counts:")
    for key, value in sorted(flag_counts.items()):
        print(f"  fmt=0x{key[0]:02X} flags=0x{key[1]:08X}: {value}")
    print("format/decoded-alpha-shape counts:")
    for key, value in sorted(alpha_shape_counts.items()):
        print(f"  fmt=0x{key[0]:02X} {key[1]}: {value}")
    print(f"Vita flags low16 == PS3 decoded alpha max/min: {flag_matches_alpha_range}/{len(ps3)}")
    print(f"Vita flags low16 == PS3 base-mip alpha max/min: {flag_matches_base_alpha_range}/{len(ps3)}")
    print(f"BC3 textures with exact alpha payload after swizzle: {alpha_payload_exact}/{counts['fmt_87']}")
    if failures:
        print("first payload failures:")
        for failure in failures:
            print(" ", failure)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
