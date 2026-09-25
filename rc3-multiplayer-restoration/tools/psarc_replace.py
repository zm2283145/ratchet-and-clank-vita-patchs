#!/usr/bin/env python3
"""Create a PSARC copy with selected entries replaced, preserving block counts.

Replacement files are zero-padded to the original uncompressed length. This
keeps every entry's block index and the TOC size stable, while offsets and
compressed block sizes are rewritten in the output copy. The source archive
is never modified.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path

from psarc_select import Psarc


@dataclass(frozen=True)
class Segment:
    zindex: int
    length: int
    offset: int


def stored_block_size(archive: Psarc, block_index: int, expected: int) -> int:
    compressed = archive.compressed_sizes[block_index]
    return compressed or expected


def segment_stored_size(archive: Psarc, segment: Segment) -> int:
    remaining = segment.length
    block = segment.zindex
    total = 0
    while remaining:
        expected = min(archive.block_size, remaining)
        total += stored_block_size(archive, block, expected)
        remaining -= expected
        block += 1
    return total


def encode_blocks(data: bytes, block_size: int) -> tuple[list[bytes], list[int]]:
    blocks: list[bytes] = []
    sizes: list[int] = []
    for start in range(0, len(data), block_size):
        raw = data[start:start + block_size]
        compressed = zlib.compress(raw, 9)
        if len(compressed) < len(raw):
            blocks.append(compressed)
            sizes.append(len(compressed))
        else:
            blocks.append(raw)
            sizes.append(0)
    return blocks, sizes


def hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--replace", action="append", nargs=2, metavar=("ARCHIVE_PATH", "FILE"),
        required=True, help="replace one manifest path; may be repeated",
    )
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    archive = Psarc(args.source)
    try:
        if archive.algorithm != "zlib":
            raise SystemExit(f"replacement currently supports zlib PSARC, not {archive.algorithm}")

        name_to_index = {name.lower(): index for index, name in enumerate(archive.names, 1)}
        replacements: dict[int, tuple[str, bytes]] = {}
        report_entries: list[dict[str, object]] = []
        for archive_name, file_name in args.replace:
            normalized = archive_name.replace("\\", "/").lstrip("/").lower()
            if normalized not in name_to_index:
                raise SystemExit(f"entry not found: {archive_name}")
            index = name_to_index[normalized]
            original_length = archive.entries[index].length
            replacement = Path(file_name).read_bytes()
            if len(replacement) > original_length:
                raise SystemExit(
                    f"replacement for {archive_name} is {len(replacement):,} bytes; "
                    f"original entry is only {original_length:,} bytes"
                )
            padded = replacement + bytes(original_length - len(replacement))
            replacements[index] = (archive.names[index - 1], padded)
            report_entries.append({
                "name": archive.names[index - 1],
                "replacement": str(Path(file_name).resolve()),
                "payload_length": len(replacement),
                "padded_length": original_length,
                "payload_sha256": hashlib.sha256(replacement).hexdigest(),
            })

        # Deduplicated entries share the same source segment. Refuse accidental
        # replacement of only one alias because that would alter both names.
        keys_by_index = [Segment(e.zindex, e.length, e.offset) for e in archive.entries]
        aliases: dict[Segment, list[int]] = {}
        for index, key in enumerate(keys_by_index):
            aliases.setdefault(key, []).append(index)
        for index in replacements:
            shared = aliases[keys_by_index[index]]
            if len(shared) != 1:
                raise SystemExit(
                    f"{archive.names[index - 1]} shares its data with {len(shared)} TOC entries; "
                    "refusing an ambiguous replacement"
                )

        unique_segments = sorted(aliases, key=lambda item: item.offset)
        first_data_offset = unique_segments[0].offset
        archive.stream.seek(0)
        prefix = bytearray(archive.stream.read(first_data_offset))

        header = prefix[:32]
        toc_length, entry_size, entry_count, block_size, _flags = struct.unpack(">5I", header[12:32])
        size_width = max(1, ((block_size - 1).bit_length() + 7) // 8)
        size_table_offset = 32 + entry_size * entry_count
        new_sizes = list(archive.compressed_sizes)
        new_offsets: dict[int, int] = {}

        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("wb") as output:
            output.write(prefix)
            current_offset = first_data_offset
            for segment in unique_segments:
                new_offsets[segment.offset] = current_offset
                member_indexes = aliases[segment]
                replacement_index = next((i for i in member_indexes if i in replacements), None)
                if replacement_index is not None:
                    _name, replacement_data = replacements[replacement_index]
                    blocks, sizes = encode_blocks(replacement_data, archive.block_size)
                    expected_blocks = (segment.length + archive.block_size - 1) // archive.block_size
                    if len(blocks) != expected_blocks:
                        raise RuntimeError("replacement changed the PSARC block count")
                    for local_index, (block, compressed_size) in enumerate(zip(blocks, sizes)):
                        output.write(block)
                        new_sizes[segment.zindex + local_index] = compressed_size
                        current_offset += len(block)
                else:
                    stored_size = segment_stored_size(archive, segment)
                    archive.stream.seek(segment.offset)
                    remaining = stored_size
                    while remaining:
                        chunk = archive.stream.read(min(1024 * 1024, remaining))
                        if not chunk:
                            raise EOFError("source archive ended inside an entry")
                        output.write(chunk)
                        current_offset += len(chunk)
                        remaining -= len(chunk)

            # Patch entry offsets and block-size table after all output offsets
            # are known. Lengths and zindices intentionally remain unchanged.
            for index, entry in enumerate(archive.entries):
                entry_offset = 32 + index * entry_size
                output.seek(entry_offset + 25)
                output.write(new_offsets[entry.offset].to_bytes(5, "big"))
            output.seek(size_table_offset)
            for size in new_sizes:
                output.write(size.to_bytes(size_width, "big"))

        result = {
            "source": str(args.source.resolve()),
            "output": str(args.output.resolve()),
            "source_length": args.source.stat().st_size,
            "output_length": args.output.stat().st_size,
            "replacements": report_entries,
            "output_sha256": hash_file(args.output),
        }
        report_path = args.report or args.output.with_suffix(args.output.suffix + ".json")
        report_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2))
        return 0
    finally:
        archive.close()


if __name__ == "__main__":
    raise SystemExit(main())
