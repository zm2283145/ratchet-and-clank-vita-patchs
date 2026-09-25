#!/usr/bin/env python3
"""Disassemble an address range from a big-endian PS3 PPU ELF."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools" / "python312"))

from capstone import CS_ARCH_PPC, CS_MODE_64, CS_MODE_BIG_ENDIAN, Cs
from elftools.elf.elffile import ELFFile


def number(text: str) -> int:
    return int(text, 0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("start", type=number)
    parser.add_argument("end", type=number)
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        segment = next(
            segment for segment in elf.iter_segments()
            if segment["p_type"] == "PT_LOAD"
            and int(segment["p_flags"]) & 1
            and int(segment["p_vaddr"]) <= args.start <
                int(segment["p_vaddr"]) + int(segment["p_filesz"])
        )
        file_offset = int(segment["p_offset"]) + args.start - int(segment["p_vaddr"])

    md = Cs(CS_ARCH_PPC, CS_MODE_64 | CS_MODE_BIG_ENDIAN)
    md.skipdata = True
    code = blob[file_offset:file_offset + args.end - args.start]
    for insn in md.disasm(code, args.start):
        print(f"{insn.address:#010x}  {insn.bytes.hex():<10}  {insn.mnemonic:<10} {insn.op_str}")


if __name__ == "__main__":
    main()
