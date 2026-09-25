#!/usr/bin/env python3
"""Build a diagnostic RC3 engine using a known-good Vita texture table/VRAM.

This does not convert the source textures.  It is intended only to separate
level-geometry loading failures from VRAM-format failures during early tests.
"""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


def u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("converted_engine", type=Path)
    parser.add_argument("donor_vita_engine", type=Path)
    parser.add_argument("donor_vita_vram", type=Path)
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args()

    engine = bytearray(args.converted_engine.read_bytes())
    donor_engine = args.donor_vita_engine.read_bytes()
    donor_vram = args.donor_vita_vram.read_bytes()

    target_texture_pointer = u32(engine, 0x54)
    donor_texture_pointer = u32(donor_engine, 0x54)
    donor_texture_count = u32(donor_engine, 0x58)
    if not (0x84 <= target_texture_pointer <= len(engine)):
        raise SystemExit("converted engine has an invalid texture pointer")
    if not (0x84 <= donor_texture_pointer < len(donor_engine)):
        raise SystemExit("donor engine has an invalid texture pointer")

    # In the Vita port the texture metadata occupies the final engine section,
    # including a small fixed trailer. Keep the converted level's preceding
    # sections and graft the donor's complete, GPU-valid final section.
    output_engine = engine[:target_texture_pointer] + donor_engine[donor_texture_pointer:]
    struct.pack_into("<I", output_engine, 0x58, donor_texture_count)

    args.output_directory.mkdir(parents=True, exist_ok=True)
    engine_path = args.output_directory / "engine.ps3"
    vram_path = args.output_directory / "vram.ps3"
    engine_path.write_bytes(output_engine)
    vram_path.write_bytes(donor_vram)
    report = {
        "status": "diagnostic-surrogate-textures",
        "warning": "Textures come from another Vita level and will be visually incorrect.",
        "converted_engine": str(args.converted_engine.resolve()),
        "donor_engine": str(args.donor_vita_engine.resolve()),
        "donor_vram": str(args.donor_vita_vram.resolve()),
        "texture_pointer": target_texture_pointer,
        "texture_count": donor_texture_count,
        "engine_length": len(output_engine),
        "vram_length": len(donor_vram),
    }
    (args.output_directory / "surrogate_vram_report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
