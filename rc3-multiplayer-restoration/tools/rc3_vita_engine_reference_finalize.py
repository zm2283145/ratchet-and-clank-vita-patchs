#!/usr/bin/env python3
"""Finalize a converted RC3 Vita engine with a matching retail texture region.

This is a hardware-validation helper, not a general PS3-to-Vita texture
converter. It keeps the converted geometry/model portion and copies the
0x74-byte Vita texture records plus the platform-specific trailing data from
the matching retail engine.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("converted", type=Path)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    converted = args.converted.read_bytes()
    reference = args.reference.read_bytes()
    converted_texture = u32(converted, 0x54)
    reference_texture = u32(reference, 0x54)
    converted_count = u32(converted, 0x58)
    reference_count = u32(reference, 0x58)

    if converted_count != reference_count:
        raise SystemExit(
            f"texture count mismatch: converted={converted_count}, "
            f"reference={reference_count}"
        )
    if converted_texture > len(converted) or reference_texture > len(reference):
        raise SystemExit("texture pointer lies outside an input file")
    if converted_texture > reference_texture:
        raise SystemExit("converted non-texture region is larger than the reference layout")

    result = bytearray(converted[:converted_texture])
    result.extend(bytes(reference_texture - converted_texture))
    result.extend(reference[reference_texture:])
    struct.pack_into("<I", result, 0x54, reference_texture)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)

    prefix_differences = sum(
        left != right
        for left, right in zip(result[:reference_texture], reference[:reference_texture])
    )
    report = {
        "converted": str(args.converted.resolve()),
        "reference": str(args.reference.resolve()),
        "output": str(args.output.resolve()),
        "texture_count": reference_count,
        "converted_texture_pointer": converted_texture,
        "reference_texture_pointer": reference_texture,
        "inserted_padding": reference_texture - converted_texture,
        "output_length": len(result),
        "reference_length": len(reference),
        "prefix_differences": prefix_differences,
        "texture_region_exact": result[reference_texture:] == reference[reference_texture:],
        "sha256": hashlib.sha256(result).hexdigest(),
    }
    report_path = args.report or args.output.with_suffix(args.output.suffix + ".json")
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
