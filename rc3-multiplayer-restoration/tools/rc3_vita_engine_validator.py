#!/usr/bin/env python3
"""Structural validator for converted RC3 Vita engine/engine_vert pairs."""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path


POINTER_FIELDS = {
    "moby_models": 0x00,
    "moby_occlusion": 0x04,
    "skybox": 0x10,
    "collision": 0x14,
    "player_animations": 0x18,
    "tie_models": 0x1C,
    "ties": 0x24,
    "shrub_models": 0x2C,
    "shrubs": 0x34,
    "terrain": 0x3C,
    "precipitation": 0x40,
    "sound": 0x48,
    "gadgets": 0x4C,
    "textures": 0x54,
    "lights": 0x5C,
    "light_config": 0x64,
    "texture_menu": 0x68,
    "texture_2d": 0x70,
    "ui": 0x74,
}


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def i16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def require_range(errors: list[str], label: str, offset: int, size: int, length: int) -> None:
    if offset < 0 or size < 0 or offset + size > length:
        errors.append(f"{label}: 0x{offset:X}+0x{size:X} exceeds 0x{length:X}")


def texture_index_count(engine: bytes, pointer: int, count: int, stride: int) -> int:
    maximum = 0
    for index in range(count):
        record = pointer + index * stride
        start_offset = 8 if stride == 0x18 else 4
        start = u32(engine, record + start_offset)
        size = u32(engine, record + start_offset + 4)
        maximum = max(maximum, start + size)
    return maximum


def validate(engine: bytes, vertices: bytes) -> dict[str, object]:
    errors: list[str] = []
    warnings: list[str] = []
    counts: dict[str, int] = {}

    if len(engine) < 0x84:
        return {"valid": False, "errors": ["engine file is smaller than its header"]}

    pointers = {name: u32(engine, offset) for name, offset in POINTER_FIELDS.items()}
    for name, pointer in pointers.items():
        if pointer and not (0x84 <= pointer < len(engine)):
            errors.append(f"header pointer {name}=0x{pointer:X} is outside engine.ps3")

    # Terrain headers reference metadata in engine.ps3 and all GPU streams in engine_vert.ps3.
    terrain = pointers["terrain"]
    if terrain:
        require_range(errors, "terrain header", terrain, 0x70, len(engine))
        if terrain + 0x70 <= len(engine):
            fragment_pointer = u32(engine, terrain)
            fragment_count = u16(engine, terrain + 6)
            counts["terrain_fragments"] = fragment_count
            require_range(errors, "terrain fragment table", fragment_pointer, fragment_count * 0x30, len(engine))
            stream_pointers = {
                "vertices": u32(engine, terrain + 0x08),
                "colors": u32(engine, terrain + 0x18),
                "uvs": u32(engine, terrain + 0x28),
                "indices": u32(engine, terrain + 0x38),
            }
            maximum_vertex = 0
            maximum_index = 0
            if fragment_pointer + fragment_count * 0x30 <= len(engine):
                for index in range(fragment_count):
                    record = fragment_pointer + index * 0x30
                    tex_pointer = u32(engine, record + 0x10)
                    tex_count = u32(engine, record + 0x14)
                    vertex_start = u16(engine, record + 0x18)
                    vertex_count = u16(engine, record + 0x1A)
                    require_range(errors, f"terrain[{index}] textures", tex_pointer, tex_count * 0x10, len(engine))
                    maximum_vertex = max(maximum_vertex, vertex_start + vertex_count)
                    if tex_pointer + tex_count * 0x10 <= len(engine):
                        maximum_index = max(maximum_index, texture_index_count(engine, tex_pointer, tex_count, 0x10))
            for name, stride in (("vertices", 0x10), ("colors", 4), ("uvs", 8)):
                require_range(errors, f"terrain {name}", stream_pointers[name], maximum_vertex * stride, len(vertices))
            require_range(errors, "terrain indices", stream_pointers["indices"], maximum_index * 2, len(vertices))

    # Tie and shrub model headers are top-level fixed-size tables.
    for kind, pointer_name, count_offset, texture_stride in (
        ("tie", "tie_models", 0x20, 0x18),
        ("shrub", "shrub_models", 0x30, 0x10),
    ):
        pointer = pointers[pointer_name]
        count = u32(engine, count_offset)
        counts[f"{kind}_models"] = count
        require_range(errors, f"{kind} model table", pointer, count * 0x40, len(engine))
        if pointer + count * 0x40 <= len(engine):
            for index in range(count):
                record = pointer + index * 0x40
                vertex_pointer = u32(engine, record + 0x10)
                uv_pointer = u32(engine, record + 0x14)
                index_pointer = u32(engine, record + 0x18)
                texture_pointer = u32(engine, record + 0x1C)
                vertex_count = u16(engine, record + 0x26)
                texture_count = u16(engine, record + 0x28)
                require_range(errors, f"{kind}[{index}] vertices", vertex_pointer, vertex_count * 0x10, len(vertices))
                require_range(errors, f"{kind}[{index}] uvs", uv_pointer, vertex_count * 8, len(vertices))
                require_range(errors, f"{kind}[{index}] textures", texture_pointer, texture_count * texture_stride, len(engine))
                if texture_pointer + texture_count * texture_stride <= len(engine):
                    index_count = texture_index_count(engine, texture_pointer, texture_count, texture_stride)
                    require_range(errors, f"{kind}[{index}] indices", index_pointer, index_count * 2, len(vertices))

    # Moby models have engine-relative metadata and absolute engine_vert mesh pointers.
    moby_table = pointers["moby_models"]
    if moby_table:
        require_range(errors, "moby table header", moby_table, 4, len(engine))
        if moby_table + 4 <= len(engine):
            moby_count = u32(engine, moby_table)
            counts["moby_models"] = moby_count
            require_range(errors, "moby table", moby_table + 4, moby_count * 8, len(engine))
            if moby_table + 4 + moby_count * 8 <= len(engine):
                mesh_count = 0
                for index in range(moby_count):
                    model_pointer = u32(engine, moby_table + 8 + index * 8)
                    if not model_pointer:
                        continue
                    require_range(errors, f"moby[{index}] header", model_pointer, 0x48, len(engine))
                    if model_pointer + 0x48 > len(engine):
                        continue
                    mesh_relative = u32(engine, model_pointer)
                    if not mesh_relative:
                        continue
                    mesh_count += 1
                    mesh = model_pointer + mesh_relative
                    require_range(errors, f"moby[{index}] mesh", mesh, 0x20, len(engine))
                    if mesh + 0x20 > len(engine):
                        continue
                    tex_count = u32(engine, mesh)
                    metal_tex_count = u32(engine, mesh + 4)
                    tex_pointer = model_pointer + u32(engine, mesh + 8)
                    metal_tex_pointer = model_pointer + u32(engine, mesh + 12)
                    vertex_pointer = u32(engine, mesh + 0x10)
                    index_pointer = u32(engine, mesh + 0x14)
                    vertex_count = u16(engine, mesh + 0x18)
                    metal_vertex_count = u16(engine, mesh + 0x1A)
                    require_range(errors, f"moby[{index}] textures", tex_pointer, tex_count * 0x10, len(engine))
                    require_range(errors, f"moby[{index}] metal textures", metal_tex_pointer, metal_tex_count * 0x10, len(engine))
                    require_range(
                        errors, f"moby[{index}] vertices", vertex_pointer,
                        vertex_count * 0x20 + metal_vertex_count * 0x18, len(vertices),
                    )
                    index_count = 0
                    if tex_pointer + tex_count * 0x10 <= len(engine):
                        index_count += texture_index_count(engine, tex_pointer, tex_count, 0x10)
                    if metal_tex_pointer + metal_tex_count * 0x10 <= len(engine):
                        index_count += texture_index_count(engine, metal_tex_pointer, metal_tex_count, 0x10)
                    require_range(errors, f"moby[{index}] indices", index_pointer, index_count * 2, len(vertices))
                counts["moby_meshes"] = mesh_count
    else:
        warnings.append("no moby model table")

    return {
        "valid": not errors,
        "engine_length": len(engine),
        "engine_vert_length": len(vertices),
        "counts": counts,
        "pointers": pointers,
        "errors": errors,
        "warnings": warnings,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("engine", type=Path)
    parser.add_argument("engine_vert", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    try:
        result = validate(args.engine.read_bytes(), args.engine_vert.read_bytes())
    except (OSError, struct.error) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if result["valid"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
