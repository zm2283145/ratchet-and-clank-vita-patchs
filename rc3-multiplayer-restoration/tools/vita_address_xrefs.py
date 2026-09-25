#!/usr/bin/env python3
"""Find Thumb MOVW/MOVT address constructions near requested targets."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools"))

from capstone import CS_ARCH_ARM, CS_MODE_THUMB, Cs
from capstone.arm import ARM_OP_IMM, ARM_OP_MEM, ARM_OP_REG
from elftools.elf.elffile import ELFFile


def number(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("targets", nargs="+", type=number)
    parser.add_argument("--radius", type=number, default=0x100)
    parser.add_argument("--stores-only", action="store_true",
                        help="Only show address constructions followed by a store through that register")
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        load = next(segment for segment in elf.iter_segments()
                    if segment["p_type"] == "PT_LOAD" and
                    segment["p_flags"] & 1)
        file_start = int(load["p_offset"])
        file_end = file_start + int(load["p_filesz"])
        va_start = int(load["p_vaddr"])

    code = blob[file_start:file_end]
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    hits: list[tuple[int, int, int]] = []

    for offset in range(0, len(code) - 16, 2):
        first = next(md.disasm(code[offset:offset + 4], va_start + offset,
                               count=1), None)
        if (first is None or first.mnemonic != "movw" or
                len(first.operands) != 2 or
                first.operands[0].type != ARM_OP_REG or
                first.operands[1].type != ARM_OP_IMM):
            continue
        reg = first.operands[0].reg
        low = first.operands[1].imm & 0xffff
        cursor = offset + first.size
        for _ in range(6):
            item = next(md.disasm(code[cursor:cursor + 4],
                                  va_start + cursor, count=1), None)
            if item is None:
                break
            if (item.mnemonic == "movt" and len(item.operands) == 2 and
                    item.operands[0].type == ARM_OP_REG and
                    item.operands[0].reg == reg and
                    item.operands[1].type == ARM_OP_IMM):
                address = ((item.operands[1].imm & 0xffff) << 16) | low
                for target in args.targets:
                    if abs(address - target) <= args.radius:
                        hits.append((first.address, address, target))
                break
            cursor += item.size

    for ref, address, target in sorted(set(hits)):
        start = max(va_start, (ref - 24) & ~1)
        start_offset = file_start + start - va_start
        preview = list(md.disasm(blob[start_offset:start_offset + 88], start))
        if args.stores_only:
            address_reg = None
            constructed = False
            has_store = False
            for insn in preview:
                if insn.address == ref and insn.operands:
                    address_reg = insn.operands[0].reg
                if insn.address == ref:
                    constructed = True
                if not constructed or address_reg is None:
                    continue
                if (insn.mnemonic.startswith("str") and
                        any(op.type == ARM_OP_MEM and op.mem.base == address_reg
                            for op in insn.operands)):
                    has_store = True
                    break
            if not has_store:
                continue
        print(f"\nreference {ref:#010x} builds {address:#010x} "
              f"(target {target:#010x}, delta {target - address:+#x})")
        for insn in preview:
            marker = ">" if insn.address == ref else " "
            print(f" {marker} {insn.address:#010x}: "
                  f"{insn.mnemonic:<9} {insn.op_str}")


if __name__ == "__main__":
    main()
