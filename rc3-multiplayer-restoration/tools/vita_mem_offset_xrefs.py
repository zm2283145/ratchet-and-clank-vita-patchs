#!/usr/bin/env python3
"""Find Thumb memory operands using a requested displacement."""

from __future__ import annotations

import argparse
import sys
from collections import deque
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools"))

from capstone import CS_ARCH_ARM, CS_MODE_THUMB, Cs
from capstone.arm import ARM_OP_MEM
from elftools.elf.elffile import ELFFile


def number(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("offset", type=number)
    parser.add_argument("--context", type=int, default=8)
    parser.add_argument("--stores-only", action="store_true")
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        segment = next(item for item in elf.iter_segments()
                       if item["p_type"] == "PT_LOAD" and item["p_flags"] & 1)
        file_start = int(segment["p_offset"])
        file_end = file_start + int(segment["p_filesz"])
        va_start = int(segment["p_vaddr"])

    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    md.skipdata = True
    instructions = list(md.disasm(blob[file_start:file_end], va_start))
    hits = []
    for index, instruction in enumerate(instructions):
        if args.stores_only and not instruction.mnemonic.startswith("str"):
            continue
        if any(operand.type == ARM_OP_MEM and operand.mem.disp == args.offset
               for operand in instruction.operands):
            hits.append(index)

    print(f"Found {len(hits)} references to displacement {args.offset:#x}")
    for index in hits:
        start = max(0, index - args.context)
        end = min(len(instructions), index + args.context + 1)
        print()
        for cursor in range(start, end):
            instruction = instructions[cursor]
            marker = ">" if cursor == index else " "
            print(f"{marker} {instruction.address:#010x}: "
                  f"{instruction.mnemonic:<9} {instruction.op_str}")


if __name__ == "__main__":
    main()
