#!/usr/bin/env python3
"""Find PS3 PPU memory operands using a requested displacement."""

from __future__ import annotations

import argparse
import re
import sys
from collections import deque
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools" / "python312"))

from capstone import CS_ARCH_PPC, CS_MODE_64, CS_MODE_BIG_ENDIAN, Cs
from elftools.elf.elffile import ELFFile


MEMORY_RE = re.compile(r"(?P<disp>-?0x[0-9a-f]+|-?[0-9]+)\(r[0-9]+\)")


def number(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("offset", type=number)
    parser.add_argument("--context", type=int, default=8)
    parser.add_argument("--stores-only", action="store_true")
    parser.add_argument("--follow-store", type=number,
                        help="Find a loaded register stored to this displacement")
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        segments = [item for item in elf.iter_segments()
                    if item["p_type"] == "PT_LOAD" and item["p_flags"] & 1]

    md = Cs(CS_ARCH_PPC, CS_MODE_64 | CS_MODE_BIG_ENDIAN)
    md.skipdata = True
    history = deque(maxlen=args.context)
    found = 0
    pending = {}
    instruction_index = 0
    for segment in segments:
        start = int(segment["p_offset"])
        end = start + int(segment["p_filesz"])
        address = int(segment["p_vaddr"])
        for instruction in md.disasm(blob[start:end], address):
            instruction_index += 1
            pending = {register: state for register, state in pending.items()
                       if instruction_index - state[0] <= 80}
            matches = [number(item.group("disp")) for item in MEMORY_RE.finditer(instruction.op_str)]
            is_store = instruction.mnemonic.startswith("st")
            operands = [item.strip() for item in instruction.op_str.split(",")]
            if (instruction.mnemonic == "lwz" and len(operands) >= 2 and
                    args.offset in matches):
                pending[operands[0]] = (instruction_index, instruction)
            if (args.follow_store is not None and is_store and len(operands) >= 2 and
                    args.follow_store in matches and operands[0] in pending):
                source = pending[operands[0]][1]
                found += 1
                print(f"{source.address:#010x}: {source.mnemonic:<10} {source.op_str}")
                print(f"{instruction.address:#010x}: {instruction.mnemonic:<10} {instruction.op_str}\n")
                history.append(instruction)
                continue
            if args.follow_store is not None:
                history.append(instruction)
                continue
            if args.offset in matches and (not args.stores_only or is_store):
                found += 1
                print()
                for previous in history:
                    print(f"  {previous.address:#010x}: {previous.mnemonic:<10} {previous.op_str}")
                print(f"> {instruction.address:#010x}: {instruction.mnemonic:<10} {instruction.op_str}")
            history.append(instruction)
    print(f"\nFound {found} references to displacement {args.offset:#x}")


if __name__ == "__main__":
    main()
