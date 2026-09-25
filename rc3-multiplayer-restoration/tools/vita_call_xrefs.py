#!/usr/bin/env python3
"""List direct Thumb branch/call references to virtual addresses in a Vita ELF."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools"))

from capstone import CS_ARCH_ARM, CS_MODE_THUMB, Cs  # noqa: E402
from capstone.arm import ARM_OP_IMM  # noqa: E402
from elftools.elf.elffile import ELFFile  # noqa: E402


def number(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("targets", nargs="+", type=number)
    args = parser.parse_args()
    wanted = set(args.targets)

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        load = next(seg for seg in elf.iter_segments() if seg["p_type"] == "PT_LOAD" and seg["p_flags"] & 1)
        file_start = int(load["p_offset"])
        file_end = file_start + int(load["p_filesz"])
        va_start = int(load["p_vaddr"])

    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    refs = {target: [] for target in wanted}
    code = blob[file_start:file_end]
    for offset in range(0, len(code) - 4, 2):
        h1 = int.from_bytes(code[offset : offset + 2], "little")
        h2 = int.from_bytes(code[offset + 2 : offset + 4], "little")
        if h1 & 0xF800 != 0xF000 or h2 & 0xC000 != 0xC000:
            continue
        insn = next(md.disasm(code[offset : offset + 4], va_start + offset, count=1), None)
        if insn is None:
            continue
        if insn.mnemonic not in {"b", "bl", "blx"} or not insn.operands:
            continue
        operand = insn.operands[0]
        target = operand.imm & 0xFFFFFFFF
        if operand.type == ARM_OP_IMM and target in wanted:
            refs[target].append((insn.address, insn.mnemonic))
    for target in args.targets:
        print(f"{target:#010x}")
        for address, mnemonic in refs[target]:
            print(f"  {address:#010x} {mnemonic}")


if __name__ == "__main__":
    main()
