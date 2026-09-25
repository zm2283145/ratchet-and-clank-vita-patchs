#!/usr/bin/env python3
"""Find direct Thumb branch/call references to virtual addresses in a Vita ELF."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools"))

from capstone import CS_ARCH_ARM, CS_MODE_THUMB, Cs
from capstone.arm import ARM_OP_IMM
from elftools.elf.elffile import ELFFile


def number(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("targets", nargs="+", type=number)
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        load = next(
            seg
            for seg in elf.iter_segments()
            if seg["p_type"] == "PT_LOAD" and seg["p_flags"] & 1
        )
        file_start = int(load["p_offset"])
        file_end = file_start + int(load["p_filesz"])
        va_start = int(load["p_vaddr"])

    targets = {target & ~1 for target in args.targets}
    refs = {target: [] for target in targets}
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    md.skipdata = True
    for insn in md.disasm(blob[file_start:file_end], va_start):
        if not insn.mnemonic.startswith("b") or not insn.operands:
            continue
        operand = insn.operands[0]
        if operand.type != ARM_OP_IMM:
            continue
        target = operand.imm & ~1
        if target in refs:
            refs[target].append((insn.address, insn.mnemonic, insn.op_str))

    for target in sorted(refs):
        print(f"{target:#010x}")
        for address, mnemonic, operands in refs[target]:
            print(f"  {address:#010x}: {mnemonic:8} {operands}")


if __name__ == "__main__":
    main()
