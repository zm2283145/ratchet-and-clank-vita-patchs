#!/usr/bin/env python3
"""Convert an RC3 PS3 texture table/VRAM into the Vita texture layout.

The engine input is an already converted split-Vita engine whose final PS3-
style 0x24 texture table is replaced with native 0x74 Vita descriptors. Texture
pixels come from the original PS3 engine/vram pair; no Vita donor is required.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from rc3_vita_texture_analysis import bc3_alpha_values, mip_layout, swizzle_blocks


def u32be(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def u32le(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def align(value: int, amount: int) -> int:
    return (value + amount - 1) & ~(amount - 1)


def make_bc1_color_block(bc3_block: bytes) -> bytes:
    """Reuse a BC3 color block as opaque BC1 without changing its colors."""
    c0, c1, indices = struct.unpack_from("<HHI", bc3_block, 8)
    if c0 > c1:
        return bc3_block[8:16]
    if c0 < c1:
        # BC3 always uses four colors. BC1 only does so when c0 > c1, so swap
        # endpoints and XOR the low bit of every 2-bit selector.
        return struct.pack("<HHI", c1, c0, indices ^ 0x55555555)
    # All palette colors are identical. Force four-color BC1 while selecting
    # the unchanged endpoint for every pixel.
    if c0:
        return struct.pack("<HHI", c0, c0 - 1, 0)
    return struct.pack("<HHI", 1, 0, 0x55555555)


def texture_alpha_min(ps3_vram: bytes, pointer: int, width: int, height: int, mip_count: int) -> int:
    minimum = 255
    for _mip, _w, _h, _bw, _bh, offset, size in mip_layout(width, height, mip_count, 16):
        level = ps3_vram[pointer + offset:pointer + offset + size]
        for block_offset in range(0, len(level), 16):
            minimum = min(minimum, *bc3_alpha_values(level[block_offset:block_offset + 16]))
    return minimum


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("converted_engine", type=Path)
    parser.add_argument("ps3_engine", type=Path)
    parser.add_argument("ps3_vram", type=Path)
    parser.add_argument("output_engine", type=Path)
    parser.add_argument("output_vram", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    converted = args.converted_engine.read_bytes()
    ps3_engine = args.ps3_engine.read_bytes()
    ps3_vram = args.ps3_vram.read_bytes()
    converted_table = u32le(converted, 0x54)
    converted_count = u32le(converted, 0x58)
    ps3_table = u32be(ps3_engine, 0x54)
    ps3_count = u32be(ps3_engine, 0x58)
    if converted_count != ps3_count:
        raise SystemExit(f"texture count mismatch: converted={converted_count}, PS3={ps3_count}")

    vita_table = align(converted_table, 0x100)
    engine = bytearray(converted[:converted_table])
    engine.extend(bytes(vita_table - len(engine)))
    descriptors = bytearray((ps3_count + 64) * 0x74)
    vram = bytearray()
    bc1_count = 0
    bc3_count = 0

    for index in range(ps3_count):
        source = ps3_table + index * 0x24
        source_pointer = u32be(ps3_engine, source)
        mip_count = ps3_engine[source + 5]
        source_format = ps3_engine[source + 6]
        if source_format != 0x88:
            raise SystemExit(f"texture {index}: expected PS3 BC3 format 0x88, got 0x{source_format:02X}")
        width, height = struct.unpack_from(">HH", ps3_engine, source + 0x18)
        alpha_metadata = ps3_engine[source + 0x20:source + 0x22]
        alpha_min = texture_alpha_min(ps3_vram, source_pointer, width, height, mip_count)
        use_bc1 = alpha_metadata == bytes((255, 255)) or alpha_min >= 254
        block_size = 8 if use_bc1 else 16
        fmt = 0x85 if use_bc1 else 0x87
        bc1_count += use_bc1
        bc3_count += not use_bc1

        pointer = len(vram)
        payload = bytearray()
        for _mip, _w, _h, bw, bh, offset, size in mip_layout(width, height, mip_count, 16):
            level = ps3_vram[source_pointer + offset:source_pointer + offset + size]
            if len(level) != size:
                raise SystemExit(f"texture {index}: truncated PS3 mip payload")
            if use_bc1:
                linear = b"".join(make_bc1_color_block(level[o:o + 16]) for o in range(0, size, 16))
            else:
                linear = level
            payload.extend(swizzle_blocks(linear, bw, bh, block_size))
        allocation_size = align(len(payload), 0x10)
        payload.extend(bytes(allocation_size - len(payload)))
        vram.extend(payload)

        dest = index * 0x74
        struct.pack_into("<I", descriptors, dest + 0x00, pointer)
        descriptors[dest + 0x07] = fmt
        struct.pack_into("<I", descriptors, dest + 0x08, u32be(ps3_engine, source + 0x08))
        struct.pack_into("<I", descriptors, dest + 0x0C, mip_count)
        # Vita's descriptor convention transposes all non-square dimensions.
        struct.pack_into("<HH", descriptors, dest + 0x18, height, width)
        struct.pack_into("<I", descriptors, dest + 0x1C, allocation_size)
        if use_bc1:
            descriptors[dest + 0x20:dest + 0x24] = bytes((255, 255, 0, 0))
        else:
            # This field has the same byte order and alpha-range role on PS3.
            descriptors[dest + 0x20:dest + 0x24] = ps3_engine[source + 0x20:source + 0x24]

    engine.extend(descriptors)
    struct.pack_into("<I", engine, 0x54, vita_table)

    args.output_engine.parent.mkdir(parents=True, exist_ok=True)
    args.output_vram.parent.mkdir(parents=True, exist_ok=True)
    args.output_engine.write_bytes(engine)
    args.output_vram.write_bytes(vram)
    report = {
        "converted_engine": str(args.converted_engine.resolve()),
        "ps3_engine": str(args.ps3_engine.resolve()),
        "ps3_vram": str(args.ps3_vram.resolve()),
        "output_engine": str(args.output_engine.resolve()),
        "output_vram": str(args.output_vram.resolve()),
        "texture_count": ps3_count,
        "bc1_count": bc1_count,
        "bc3_count": bc3_count,
        "texture_table_pointer": vita_table,
        "engine_size": len(engine),
        "vram_size": len(vram),
        "engine_sha256": hashlib.sha256(engine).hexdigest(),
        "vram_sha256": hashlib.sha256(vram).hexdigest(),
    }
    report_path = args.report or args.output_engine.with_suffix(args.output_engine.suffix + ".json")
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
