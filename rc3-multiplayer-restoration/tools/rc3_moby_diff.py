#!/usr/bin/env python3
"""Report byte differences between matching RC3 Vita moby model records."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def populated_models(data: bytes) -> list[tuple[int, int]]:
    table = u32(data, 0)
    count = u32(data, table)
    result = []
    for index in range(count):
        model_id = u16(data, table + 4 + index * 8)
        model_offset = u32(data, table + 8 + index * 8)
        if model_offset:
            result.append((model_id, model_offset))
    return result


def diff_count(left: bytes, right: bytes, start: int, end: int) -> int:
    return sum(a != b for a, b in zip(left[start:end], right[start:end]))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("converted", type=Path)
    parser.add_argument("retail", type=Path)
    parser.add_argument("moby_ids", nargs="+", type=lambda value: int(value, 0))
    args = parser.parse_args()

    converted = args.converted.read_bytes()
    retail = args.retail.read_bytes()
    models = populated_models(retail)
    wanted = set(args.moby_ids)
    report = []

    for position, (model_id, start) in enumerate(models):
        if model_id not in wanted:
            continue
        end = models[position + 1][1] if position + 1 < len(models) else u32(retail, 0x48)
        relative_end = end - start
        bangle = u16(retail, start + 0x2C) * 0x10
        corncob = u16(retail, start + 0x2E) * 0x10
        boundaries = {
            "header": 0,
            "mesh": u32(retail, start),
            "bangle": bangle,
            "collision": u32(retail, start + 0x10),
            "sound": u32(retail, start + 0x28),
            "attachment": u32(retail, start + 0x1C),
            "bone_matrix": u32(retail, start + 0x14),
            "bone_data": u32(retail, start + 0x18),
            "animation": u32(retail, start + 0x48) if retail[start + 0x0C] else 0,
            "corncob": corncob,
            "end": relative_end,
        }
        ordered = sorted({value for value in boundaries.values() if 0 <= value <= relative_end})
        ranges = []
        for range_start, range_end in zip(ordered, ordered[1:]):
            names = [name for name, value in boundaries.items() if value == range_start]
            ranges.append({
                "start": hex(range_start),
                "end": hex(range_end),
                "labels": names,
                "differences": diff_count(converted, retail, start + range_start, start + range_end),
            })

        bangle_detail = None
        if bangle:
            mask = u16(retail, start + bangle + 2)
            pointers = [u32(retail, start + bangle + 4 + index * 4) for index in range(15)]
            active_pointers = [pointer for index, pointer in enumerate(pointers) if mask & (1 << index)]
            slots = []
            for index, pointer in enumerate(active_pointers):
                slot_start = bangle + 0x40 + index * 0x10
                slots.append({
                    "offset": hex(slot_start),
                    "converted": converted[start + slot_start:start + slot_start + 0x10].hex(" "),
                    "retail": retail[start + slot_start:start + slot_start + 0x10].hex(" "),
                })
            bangle_detail = {
                "mask": hex(mask),
                "submodel_pointers": [hex(pointer) for pointer in active_pointers],
                "unknown_slots": slots,
            }

        report.append({
            "id": hex(model_id),
            "offset": hex(start),
            "length": relative_end,
            "differences": diff_count(converted, retail, start, end),
            "boundaries": {name: hex(value) for name, value in boundaries.items()},
            "ranges": ranges,
            "bangle": bangle_detail,
        })

    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
