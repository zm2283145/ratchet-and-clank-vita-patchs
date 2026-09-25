#!/usr/bin/env python3
"""Locate RC3 PS3 lobby screen functions and their direct callers.

The decrypted PPU ELF is an ELFv1-style executable whose function descriptor
table stores 32-bit code addresses followed by the common TOC address.  Lobby
screen functions retain diagnostic strings, so materialized string addresses
provide reliable anchors even though normal symbols were stripped.
"""

from __future__ import annotations

import argparse
from bisect import bisect_right
from pathlib import Path


LOAD_BIAS = 0x10000
TOC_ADDRESS = 0x00C0A4F0
CODE_LIMIT = 0x00AEE900

ANCHORS = (
    "Constructing LobbyGUIScreenMain",
    "Entering LobbyGUIScreenMain",
    "DONE CREATING LobbyGUIScreenLocalProfiles",
    "Entering LobbyGUIScreenLocalProfiles",
    "Constructing LobbyGUIScreenCreate",
    "Entering LobbyGUIScreenCreate",
    "Constructing LobbyGUIScreenStaging",
    "Entering LobbyGUIScreenStaging",
)


def function_starts(blob: bytes) -> list[int]:
    toc = TOC_ADDRESS.to_bytes(4, "big")
    starts: set[int] = set()
    for offset in range(0, len(blob) - 8, 4):
        if blob[offset + 4 : offset + 8] != toc:
            continue
        address = int.from_bytes(blob[offset : offset + 4], "big")
        if LOAD_BIAS <= address < CODE_LIMIT and address % 4 == 0:
            starts.add(address)
    return sorted(starts)


def owner(starts: list[int], address: int) -> int | None:
    index = bisect_right(starts, address) - 1
    return starts[index] if index >= 0 else None


def direct_calls(blob: bytes, starts: list[int]) -> dict[int, list[tuple[int, int | None]]]:
    result: dict[int, list[tuple[int, int | None]]] = {}
    for offset in range(0, min(len(blob) - 4, CODE_LIMIT - LOAD_BIAS), 4):
        word = int.from_bytes(blob[offset : offset + 4], "big")
        if word >> 26 != 18 or not (word & 1):
            continue
        address = offset + LOAD_BIAS
        displacement = word & 0x03FFFFFC
        if displacement & 0x02000000:
            displacement -= 0x04000000
        target = displacement if word & 2 else address + displacement
        result.setdefault(target, []).append((address, owner(starts, address)))
    return result


def address_materializations(blob: bytes, target: int) -> list[int]:
    hits: set[int] = set()
    # Some diagnostic strings have a one-byte prefix in front of the printable
    # text, so consider a small neighborhood around the visible string.
    for candidate in range(target - 16, target + 5):
        high = ((candidate + 0x8000) >> 16) & 0xFFFF
        low = candidate & 0xFFFF
        for offset in range(0, min(len(blob) - 32, CODE_LIMIT - LOAD_BIAS), 4):
            first = int.from_bytes(blob[offset : offset + 4], "big")
            if first >> 26 != 15 or (first & 0xFFFF) != high:
                continue
            register = (first >> 21) & 31
            for second_offset in range(offset + 4, offset + 32, 4):
                second = int.from_bytes(blob[second_offset : second_offset + 4], "big")
                if second >> 26 not in (12, 14):  # addic / addi
                    continue
                if (second & 0xFFFF) != low:
                    continue
                if ((second >> 21) & 31) != register:
                    continue
                hits.add(offset + LOAD_BIAS)
    return sorted(hits)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    starts = function_starts(blob)
    calls = direct_calls(blob, starts)
    print(f"function descriptors: {len(starts)}")

    for label in ANCHORS:
        needle = label.encode("ascii")
        try:
            file_offset = blob.index(needle)
        except ValueError:
            print(f"{label}: absent")
            continue
        virtual_address = file_offset + LOAD_BIAS
        references = address_materializations(blob, virtual_address)
        print(f"\n{label}: string=0x{virtual_address:08X}")
        if not references:
            print("  no direct address materialization found")
            continue
        for reference in references:
            function = owner(starts, reference)
            print(f"  reference=0x{reference:08X} function=0x{function:08X}")
            for call_site, caller in calls.get(function, []):
                caller_text = f"0x{caller:08X}" if caller is not None else "unknown"
                print(f"    direct caller={caller_text} at 0x{call_site:08X}")


if __name__ == "__main__":
    main()
