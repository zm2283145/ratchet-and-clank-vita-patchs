#!/usr/bin/env python3
"""Disassemble a virtual-address range from the RX segment of a Vita ELF."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools"))

from capstone import CS_ARCH_ARM, CS_MODE_THUMB, Cs  # noqa: E402
from elftools.elf.elffile import ELFFile  # noqa: E402


def number(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("start", type=number)
    parser.add_argument("end", type=number)
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        load = next(seg for seg in elf.iter_segments() if seg["p_type"] == "PT_LOAD" and seg["p_flags"] & 1)
        file_start = int(load["p_offset"])
        va_start = int(load["p_vaddr"])

    start_offset = file_start + args.start - va_start
    end_offset = file_start + args.end - va_start
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.skipdata = True
    for insn in md.disasm(blob[start_offset:end_offset], args.start):
        raw = insn.bytes.hex()
        print(f"{insn.address:#010x}  {raw:<10}  {insn.mnemonic:<9} {insn.op_str}")


if __name__ == "__main__":
    main()
