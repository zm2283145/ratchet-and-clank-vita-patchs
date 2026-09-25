#!/usr/bin/env python3
"""List direct PS3 PPU calls to requested virtual addresses."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools" / "python312"))

from capstone import CS_ARCH_PPC, CS_MODE_64, CS_MODE_BIG_ENDIAN, Cs
from elftools.elf.elffile import ELFFile


def number(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("targets", nargs="+", type=number)
    args = parser.parse_args()
    wanted = set(args.targets)
    refs = {target: [] for target in wanted}

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        segments = [item for item in elf.iter_segments()
                    if item["p_type"] == "PT_LOAD" and item["p_flags"] & 1]

    md = Cs(CS_ARCH_PPC, CS_MODE_64 | CS_MODE_BIG_ENDIAN)
    for segment in segments:
        start = int(segment["p_offset"])
        end = start + int(segment["p_filesz"])
        address = int(segment["p_vaddr"])
        for instruction in md.disasm(blob[start:end], address):
            if instruction.mnemonic not in {"bl", "bla"}:
                continue
            try:
                target = int(instruction.op_str, 0)
            except ValueError:
                continue
            if target in wanted:
                refs[target].append(instruction.address)

    for target in args.targets:
        print(f"{target:#010x}")
        for address in refs[target]:
            print(f"  {address:#010x} bl")


if __name__ == "__main__":
    main()
