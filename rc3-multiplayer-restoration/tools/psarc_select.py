#!/usr/bin/env python3
"""List or selectively extract files from zlib/LZMA PSARC archives."""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import lzma
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Entry:
    zindex: int
    length: int
    offset: int


class Psarc:
    def __init__(self, path: Path, base_offset: int = 0) -> None:
        self.path = path
        self.base_offset = base_offset
        self.stream = path.open("rb")
        self.stream.seek(base_offset)
        header = self.stream.read(32)
        if len(header) != 32 or header[:4] != b"PSAR":
            raise ValueError("not a PSARC archive at the requested offset")
        self.algorithm = header[8:12].rstrip(b"\0").decode("ascii")
        toc_length, entry_size, count, self.block_size, self.flags = struct.unpack(
            ">5I", header[12:32]
        )
        if entry_size < 30:
            raise ValueError(f"unsupported TOC entry size: {entry_size}")

        self.entries: list[Entry] = []
        for _ in range(count):
            raw = self.stream.read(entry_size)
            self.entries.append(
                Entry(
                    int.from_bytes(raw[16:20], "big"),
                    int.from_bytes(raw[20:25], "big"),
                    int.from_bytes(raw[25:30], "big"),
                )
            )

        # Width stores sizes below block_size; 65536-byte blocks use uint16.
        size_width = max(1, ((self.block_size - 1).bit_length() + 7) // 8)
        size_count = (toc_length - 32 - entry_size * count) // size_width
        self.compressed_sizes = [
            int.from_bytes(self.stream.read(size_width), "big")
            for _ in range(size_count)
        ]

        manifest = self.read_entry(0).decode("utf-8-sig").splitlines()
        if len(manifest) != count - 1:
            raise ValueError(
                f"manifest has {len(manifest)} names for {count - 1} entries"
            )
        self.names = [name.replace("\\", "/") for name in manifest]

    def close(self) -> None:
        self.stream.close()

    def _decompress(self, data: bytes, expected: int) -> bytes:
        if len(data) == expected:
            return data
        if self.algorithm == "zlib":
            result = zlib.decompress(data)
        elif self.algorithm == "lzma":
            result = lzma.decompress(data, format=lzma.FORMAT_ALONE)
        else:
            raise ValueError(f"unsupported compression algorithm: {self.algorithm}")
        if len(result) != expected:
            raise ValueError(f"block expanded to {len(result)}, expected {expected}")
        return result

    def read_entry(self, index: int) -> bytes:
        entry = self.entries[index]
        remaining = entry.length
        output = bytearray()
        block_index = entry.zindex
        self.stream.seek(self.base_offset + entry.offset)
        while remaining:
            expected = min(self.block_size, remaining)
            compressed = self.compressed_sizes[block_index]
            stored = compressed or expected
            data = self.stream.read(stored)
            if len(data) != stored:
                raise EOFError("archive ended inside a data block")
            output.extend(self._decompress(data, expected) if compressed else data)
            remaining -= expected
            block_index += 1
        return bytes(output)

    def matches(self, patterns: list[str]) -> list[tuple[int, str]]:
        lowered = [pattern.lower() for pattern in patterns]
        return [
            (index, name)
            for index, name in enumerate(self.names, start=1)
            if any(fnmatch.fnmatch(name.lower(), pattern) for pattern in lowered)
        ]


def number(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("patterns", nargs="+", help="case-insensitive glob patterns")
    parser.add_argument("--base-offset", type=number, default=0)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()

    archive = Psarc(args.archive, args.base_offset)
    try:
        matches = archive.matches(args.patterns)
        for index, name in matches:
            if args.out is None:
                entry = archive.entries[index]
                print(f"{entry.length:10d}  {name}")
                continue
            data = archive.read_entry(index)
            destination = args.out / Path(name.lstrip("/"))
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
            digest = hashlib.sha256(data).hexdigest()
            print(f"{len(data):10d}  {digest}  {name}")
        print(f"matched {len(matches)} file(s)")
    finally:
        archive.close()


if __name__ == "__main__":
    main()
