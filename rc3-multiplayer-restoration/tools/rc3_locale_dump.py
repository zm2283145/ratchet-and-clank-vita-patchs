#!/usr/bin/env python3
"""Inspect Insomniac RC3 locale tables from PS3 or Vita data files."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def number(value: str) -> int:
    return int(value, 0)


def choose_endian(blob: bytes) -> str:
    candidates = []
    for endian in ("<", ">"):
        count, declared_size = struct.unpack_from(f"{endian}II", blob)
        if 0 < count < 0x10000 and 8 + count * 16 <= len(blob):
            candidates.append((endian, count, declared_size))
    if len(candidates) != 1:
        raise ValueError("could not determine locale-table byte order")
    return candidates[0][0]


def decode_text(raw: bytes) -> str:
    value = raw.split(b"\0", 1)[0]
    return value.decode("utf-8", "backslashreplace")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("locale", type=Path)
    parser.add_argument("ids", nargs="*", type=number)
    parser.add_argument("--find", help="case-insensitive substring search")
    args = parser.parse_args()

    blob = args.locale.read_bytes()
    endian = choose_endian(blob)
    count, declared_size = struct.unpack_from(f"{endian}II", blob)
    records: dict[int, tuple[int, int, int, int, int]] = {}
    for index in range(count):
        offset, text_id, flag0, flag1, flag2, flag3 = struct.unpack_from(
            f"{endian}II4H", blob, 8 + index * 16
        )
        records[text_id] = (offset, flag0, flag1, flag2, flag3)

    print(
        f"endian={'little' if endian == '<' else 'big'} records={count} "
        f"declared_size=0x{declared_size:X} file_size=0x{len(blob):X}"
    )

    selected = args.ids or sorted(records)
    needle = args.find.casefold() if args.find else None
    for text_id in selected:
        record = records.get(text_id)
        if record is None:
            if args.ids:
                print(f"0x{text_id:04X}: <absent>")
            continue
        offset, flag0, flag1, flag2, flag3 = record
        if offset >= len(blob):
            text = "<invalid offset>"
        else:
            text = decode_text(blob[offset:])
        if needle is not None and needle not in text.casefold():
            continue
        print(
            f"0x{text_id:04X}: offset=0x{offset:X} "
            f"flags={flag0:04X}/{flag1:04X}/{flag2:04X}/{flag3:04X} "
            f"{text!r}"
        )


if __name__ == "__main__":
    main()
