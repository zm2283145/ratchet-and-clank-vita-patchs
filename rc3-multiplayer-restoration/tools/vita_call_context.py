#!/usr/bin/env python3
"""Print instruction context around direct Thumb calls to a target address."""

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
    parser.add_argument("target", type=number)
    parser.add_argument("--before", type=int, default=12)
    parser.add_argument("--after", type=int, default=3)
    parser.add_argument("--start", type=number)
    parser.add_argument("--end", type=number)
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        load = next(seg for seg in elf.iter_segments()
                    if seg["p_type"] == "PT_LOAD" and seg["p_flags"] & 1)
        file_start = int(load["p_offset"])
        file_end = file_start + int(load["p_filesz"])
        va_start = int(load["p_vaddr"])

    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    code = blob[file_start:file_end]
    for offset in range(0, len(code) - 4, 2):
        h1 = int.from_bytes(code[offset:offset + 2], "little")
        h2 = int.from_bytes(code[offset + 2:offset + 4], "little")
        if h1 & 0xF800 != 0xF000 or h2 & 0xC000 != 0xC000:
            continue
        insn = next(md.disasm(code[offset:offset + 4], va_start + offset,
                              count=1), None)
        if insn is None or insn.mnemonic not in ("bl", "blx") or not insn.operands:
            continue
        if (insn.operands[0].type != ARM_OP_IMM or
                (insn.operands[0].imm & 0xFFFFFFFF) != args.target):
            continue
        if args.start is not None and insn.address < args.start:
            continue
        if args.end is not None and insn.address >= args.end:
            continue
        print(f"\n=== call {insn.address:#010x} ===")
        context_bytes = args.before * 4
        context_start = max(va_start, (insn.address - context_bytes) & ~1)
        context_offset = file_start + context_start - va_start
        context_end = min(file_end, context_offset + context_bytes + 4 + args.after * 4)
        for item in md.disasm(blob[context_offset:context_end], context_start):
            marker = ">" if item.address == insn.address else " "
            print(f"{marker} {item.address:#010x} {item.mnemonic:<9} {item.op_str}")


if __name__ == "__main__":
    main()
