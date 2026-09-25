#!/usr/bin/env python3
"""Compare RC3 PS3 and Vita front-end transition tables.

The PS3 table stores function-descriptor addresses.  The Vita table stores
Thumb function pointers directly.  This reports the shared state mapping and
resolves the PS3 descriptors to their actual code entry points so removed
front-end controllers can be distinguished from retained loading states.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from elftools.elf.elffile import ELFFile


PS3_LOAD_BIAS = 0x10000
PS3_CALLBACK_TABLE = 0x00C49034
PS3_MAPPING_TABLE = 0x00C4913C
VITA_CALLBACK_TABLE = 0x81840AF4
VITA_MAPPING_TABLE = 0x81840BFC
CALLBACK_RECORDS = 11
MAPPING_RECORDS = 68


def vita_bytes(path: Path, address: int, size: int) -> bytes:
    with path.open("rb") as stream:
        elf = ELFFile(stream)
        for segment in elf.iter_segments():
            start = int(segment["p_vaddr"])
            end = start + int(segment["p_filesz"])
            if segment["p_type"] == "PT_LOAD" and start <= address < end:
                offset = address - start
                return segment.data()[offset : offset + size]
    raise ValueError(f"Vita address {address:#x} is not file-backed")


def ps3_bytes(blob: bytes, address: int, size: int) -> bytes:
    offset = address - PS3_LOAD_BIAS
    return blob[offset : offset + size]


def mappings(data: bytes, endian: str) -> list[tuple[int, int, int, int, int]]:
    return [
        struct.unpack_from(f"{endian}hhIII", data, index * 16)
        for index in range(MAPPING_RECORDS)
    ]


def callbacks(data: bytes, endian: str) -> list[tuple[int, ...]]:
    return [
        struct.unpack_from(f"{endian}6I", data, index * 24)
        for index in range(CALLBACK_RECORDS)
    ]


def resolve_ps3_descriptor(blob: bytes, descriptor: int) -> int:
    if descriptor == 0:
        return 0
    raw = ps3_bytes(blob, descriptor, 8)
    entry, toc = struct.unpack(">II", raw)
    if toc != 0x00C0A4F0:
        raise ValueError(f"invalid PS3 descriptor {descriptor:#x}")
    return entry


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("ps3_elf", type=Path)
    parser.add_argument("vita_elf", type=Path)
    args = parser.parse_args()

    ps3 = args.ps3_elf.read_bytes()
    ps3_map = mappings(
        ps3_bytes(ps3, PS3_MAPPING_TABLE, MAPPING_RECORDS * 16), ">"
    )
    vita_map = mappings(
        vita_bytes(args.vita_elf, VITA_MAPPING_TABLE, MAPPING_RECORDS * 16), "<"
    )
    print(f"transition maps identical: {ps3_map == vita_map}")

    ps3_callbacks = callbacks(
        ps3_bytes(ps3, PS3_CALLBACK_TABLE, CALLBACK_RECORDS * 24), ">"
    )
    vita_callbacks = callbacks(
        vita_bytes(args.vita_elf, VITA_CALLBACK_TABLE, CALLBACK_RECORDS * 24), "<"
    )

    for index, mapping in enumerate(ps3_map):
        previous, requested, callback_index, next_state, flags = mapping
        if 39 <= requested <= 55:
            ps3_record = ps3_callbacks[callback_index]
            vita_record = vita_callbacks[callback_index]
            ps3_entries = tuple(
                resolve_ps3_descriptor(ps3, pointer)
                for pointer in ps3_record[1:]
            )
            vita_entries = tuple(pointer & ~1 for pointer in vita_record[1:])
            print(
                f"state {requested}: map#{index} callback#{callback_index} "
                f"previous={previous} next={next_state} flags={flags}"
            )
            print(
                "  PS3  " + " ".join(f"{entry:#010x}" for entry in ps3_entries)
            )
            print(
                "  Vita " + " ".join(f"{entry:#010x}" for entry in vita_entries)
            )


if __name__ == "__main__":
    main()
