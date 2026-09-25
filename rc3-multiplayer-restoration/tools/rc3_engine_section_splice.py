#!/usr/bin/env python3
"""Splice aligned sections between two RC3 Vita engine files."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


HEADER_POINTERS = {
    "moby": 0x00,
    "player_animations": 0x18,
    "ties": 0x24,
    "shrub_models": 0x2C,
    "sound": 0x48,
    "textures": 0x54,
    "texture2d": 0x70,
}


def pointer(data: bytes, name: str) -> int:
    return struct.unpack_from("<I", data, HEADER_POINTERS[name])[0]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("converted", type=Path)
    parser.add_argument("retail", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--converted-range",
        choices=(
            "pre-moby", "moby-only", "moby-first-half", "moby-second-half",
            "moby-q3", "moby-q4", "moby-q3a", "moby-q3b",
            "moby-q3b1", "moby-q3b2",
            "moby-q3b1a", "moby-q3b1b",
            "moby-id", "moby-id-pre-corncob", "moby-id-corncob",
            "moby-id-pre-collision", "moby-id-collision-to-animation",
            "moby-id-animations", "moby-id-sounds",
            "moby-id-bone-matrices", "moby-id-bone-data",
            "ties-only", "texture2d-only", "sound-only",
        ),
        required=True,
    )
    parser.add_argument("--moby-id", type=int)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    converted = args.converted.read_bytes()
    retail = args.retail.read_bytes()
    if len(converted) != len(retail):
        raise SystemExit("inputs must have identical lengths")

    bounds = {}
    for name in HEADER_POINTERS:
        left = pointer(converted, name)
        right = pointer(retail, name)
        if left != right:
            raise SystemExit(f"{name} pointer mismatch: {left:#x} != {right:#x}")
        bounds[name] = left

    if args.converted_range == "pre-moby":
        start, end = 0, bounds["moby"]
    elif args.converted_range == "moby-only":
        start, end = bounds["moby"], bounds["sound"]
    elif args.converted_range in (
        "moby-id", "moby-id-pre-corncob", "moby-id-corncob",
        "moby-id-pre-collision", "moby-id-collision-to-animation",
        "moby-id-animations", "moby-id-sounds",
        "moby-id-bone-matrices", "moby-id-bone-data",
    ):
        if args.moby_id is None:
            raise SystemExit("--moby-id is required with --converted-range moby-id")
        table = bounds["moby"]
        count = struct.unpack_from("<I", retail, table)[0]
        populated = []
        for index in range(count):
            model_id = struct.unpack_from("<h", retail, table + 4 + index * 8)[0]
            model_pointer = struct.unpack_from("<I", retail, table + 8 + index * 8)[0]
            if model_pointer:
                populated.append((model_id, model_pointer))
        matches = [position for position, item in enumerate(populated) if item[0] == args.moby_id]
        if len(matches) != 1:
            raise SystemExit(f"expected one populated moby id {args.moby_id}, found {len(matches)}")
        position = matches[0]
        model_start = populated[position][1]
        model_end = populated[position + 1][1] if position + 1 < len(populated) else bounds["sound"]
        if args.converted_range == "moby-id":
            start, end = model_start, model_end
        else:
            # The corncob pointer is stored in 0x10-byte units relative to the
            # model header. Both source files must agree so a component splice
            # cannot accidentally invalidate the model's internal pointers.
            converted_corncob = struct.unpack_from("<H", converted, model_start + 0x2E)[0] * 0x10
            retail_corncob = struct.unpack_from("<H", retail, model_start + 0x2E)[0] * 0x10
            if converted_corncob != retail_corncob or retail_corncob == 0:
                raise SystemExit(
                    f"corncob pointer mismatch or absent for moby {args.moby_id}: "
                    f"{converted_corncob:#x} != {retail_corncob:#x}"
                )
            corncob_start = model_start + retail_corncob
            if not model_start < corncob_start < model_end:
                raise SystemExit(f"corncob pointer outside moby {args.moby_id}")
            if args.converted_range == "moby-id-pre-corncob":
                start, end = model_start, corncob_start
            elif args.converted_range == "moby-id-corncob":
                start, end = corncob_start, model_end
            else:
                converted_collision = struct.unpack_from("<I", converted, model_start + 0x10)[0]
                retail_collision = struct.unpack_from("<I", retail, model_start + 0x10)[0]
                if converted_collision != retail_collision or retail_collision == 0:
                    raise SystemExit(
                        f"collision pointer mismatch or absent for moby {args.moby_id}: "
                        f"{converted_collision:#x} != {retail_collision:#x}"
                    )
                collision_start = model_start + retail_collision

                animation_count = retail[model_start + 0x0C]
                if animation_count == 0:
                    raise SystemExit(f"moby {args.moby_id} has no animations")
                converted_animation = struct.unpack_from("<I", converted, model_start + 0x48)[0]
                retail_animation = struct.unpack_from("<I", retail, model_start + 0x48)[0]
                if converted_animation != retail_animation or retail_animation == 0:
                    raise SystemExit(
                        f"first animation pointer mismatch or absent for moby {args.moby_id}: "
                        f"{converted_animation:#x} != {retail_animation:#x}"
                    )
                animation_start = model_start + retail_animation
                if not model_start < collision_start < animation_start < corncob_start:
                    raise SystemExit(f"component pointers out of order for moby {args.moby_id}")

                relative_pointer_offsets = {
                    "sound": 0x28,
                    "attachment": 0x1C,
                    "bone matrix": 0x14,
                    "bone data": 0x18,
                }
                component_pointers = {}
                for name, pointer_offset in relative_pointer_offsets.items():
                    converted_pointer = struct.unpack_from("<I", converted, model_start + pointer_offset)[0]
                    retail_pointer = struct.unpack_from("<I", retail, model_start + pointer_offset)[0]
                    if converted_pointer != retail_pointer or retail_pointer == 0:
                        raise SystemExit(
                            f"{name} pointer mismatch or absent for moby {args.moby_id}: "
                            f"{converted_pointer:#x} != {retail_pointer:#x}"
                        )
                    component_pointers[name] = model_start + retail_pointer

                sound_start = component_pointers["sound"]
                attachment_start = component_pointers["attachment"]
                bone_matrix_start = component_pointers["bone matrix"]
                bone_data_start = component_pointers["bone data"]
                if not collision_start < sound_start < attachment_start < bone_matrix_start < bone_data_start < animation_start:
                    raise SystemExit(f"payload pointers out of order for moby {args.moby_id}")

                if args.converted_range == "moby-id-pre-collision":
                    start, end = model_start, collision_start
                elif args.converted_range == "moby-id-collision-to-animation":
                    start, end = collision_start, animation_start
                elif args.converted_range == "moby-id-sounds":
                    start, end = sound_start, attachment_start
                elif args.converted_range == "moby-id-bone-matrices":
                    start, end = bone_matrix_start, bone_data_start
                elif args.converted_range == "moby-id-bone-data":
                    start, end = bone_data_start, animation_start
                else:
                    start, end = animation_start, corncob_start
    elif args.converted_range in (
        "moby-first-half", "moby-second-half", "moby-q3", "moby-q4",
        "moby-q3a", "moby-q3b", "moby-q3b1", "moby-q3b2",
        "moby-q3b1a", "moby-q3b1b"
    ):
        table = bounds["moby"]
        count = struct.unpack_from("<I", retail, table)[0]
        model_pointers = []
        for index in range(count):
            converted_pointer = struct.unpack_from("<I", converted, table + 8 + index * 8)[0]
            retail_pointer = struct.unpack_from("<I", retail, table + 8 + index * 8)[0]
            if converted_pointer != retail_pointer:
                raise SystemExit(f"moby pointer mismatch at table index {index}")
            if retail_pointer:
                model_pointers.append(retail_pointer)
        if len(model_pointers) < 2:
            raise SystemExit("not enough populated moby models to split")
        midpoint = model_pointers[(len(model_pointers) + 1) // 2]
        second_half_count = len(model_pointers) - (len(model_pointers) + 1) // 2
        q4_index = (len(model_pointers) + 1) // 2 + (second_half_count + 1) // 2
        q4_start = model_pointers[q4_index]
        q3_count = q4_index - (len(model_pointers) + 1) // 2
        q3b_index = (len(model_pointers) + 1) // 2 + (q3_count + 1) // 2
        q3b_start = model_pointers[q3b_index]
        q3b_count = q4_index - q3b_index
        q3b2_index = q3b_index + (q3b_count + 1) // 2
        q3b2_start = model_pointers[q3b2_index]
        q3b1_count = q3b2_index - q3b_index
        q3b1b_index = q3b_index + (q3b1_count + 1) // 2
        q3b1b_start = model_pointers[q3b1b_index]
        if args.converted_range == "moby-first-half":
            start, end = table, midpoint
        elif args.converted_range == "moby-second-half":
            start, end = midpoint, bounds["sound"]
        elif args.converted_range == "moby-q3":
            start, end = midpoint, q4_start
        elif args.converted_range == "moby-q4":
            start, end = q4_start, bounds["sound"]
        elif args.converted_range == "moby-q3a":
            start, end = midpoint, q3b_start
        elif args.converted_range == "moby-q3b":
            start, end = q3b_start, q4_start
        elif args.converted_range == "moby-q3b1":
            start, end = q3b_start, q3b2_start
        elif args.converted_range == "moby-q3b2":
            start, end = q3b2_start, q4_start
        elif args.converted_range == "moby-q3b1a":
            start, end = q3b_start, q3b1b_start
        else:
            start, end = q3b1b_start, q3b2_start
    elif args.converted_range == "ties-only":
        start, end = bounds["ties"], bounds["shrub_models"]
    elif args.converted_range == "sound-only":
        start, end = bounds["sound"], bounds["player_animations"]
    else:
        start, end = bounds["texture2d"], bounds["moby"]

    result = bytearray(retail)
    result[start:end] = converted[start:end]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)

    report = {
        "converted": str(args.converted.resolve()),
        "retail": str(args.retail.resolve()),
        "output": str(args.output.resolve()),
        "converted_range": args.converted_range,
        "range_start": start,
        "range_end": end,
        "range_length": end - start,
        "sha256": hashlib.sha256(result).hexdigest(),
    }
    report_path = args.report or args.output.with_suffix(args.output.suffix + ".json")
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
