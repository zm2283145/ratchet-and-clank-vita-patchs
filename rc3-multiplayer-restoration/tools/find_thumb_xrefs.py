#!/usr/bin/env python3
"""Find Thumb PC-relative and literal-pool references in a sectionless Vita ELF."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / ".analysis-tools"))

from capstone import CS_ARCH_ARM, CS_MODE_LITTLE_ENDIAN, CS_MODE_THUMB, Cs  # noqa: E402
from capstone.arm import ARM_OP_IMM, ARM_OP_MEM, ARM_REG_PC  # noqa: E402


FILE_OFFSET = 0xC0
VIRTUAL_ADDRESS = 0x81000000
FILE_SIZE = 0x604DA4


def file_to_va(offset: int) -> int:
    return VIRTUAL_ADDRESS + offset - FILE_OFFSET


def va_to_file(address: int) -> int:
    return address - VIRTUAL_ADDRESS + FILE_OFFSET


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("targets", nargs="+", help="ASCII strings or integer addresses")
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    segment = blob[FILE_OFFSET : FILE_OFFSET + FILE_SIZE]
    targets: dict[int, str] = {}
    for raw in args.targets:
        try:
            address = int(raw, 0)
            targets[address] = raw
        except ValueError:
            needle = raw.encode("ascii")
            start = 0
            while (offset := blob.find(needle, start)) >= 0:
                targets[file_to_va(offset)] = raw
                start = offset + 1

    for address, label in targets.items():
        print(f"target {label!r}: VA 0x{address:08X}, file 0x{va_to_file(address):X}")

    # Locate pointer words first. They are commonly reached through Thumb literal loads.
    pointer_words: dict[int, int] = {}
    for offset in range(0, len(segment) - 3, 4):
        value = struct.unpack_from("<I", segment, offset)[0]
        if value in targets:
            pointer_words[VIRTUAL_ADDRESS + offset] = value
            print(f"  pointer word VA 0x{VIRTUAL_ADDRESS + offset:08X} -> 0x{value:08X}")

    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_LITTLE_ENDIAN)
    md.detail = True
    md.skipdata = True
    for insn in md.disasm(segment, VIRTUAL_ADDRESS):
        if insn.id == 0:
            continue
        hit = None
        note = ""
        for operand in insn.operands:
            if operand.type == ARM_OP_IMM and operand.imm in targets:
                hit = operand.imm
                note = "direct immediate"
                break
            if operand.type == ARM_OP_MEM and operand.mem.base == ARM_REG_PC:
                literal = ((insn.address + 4) & ~3) + operand.mem.disp
                if literal in pointer_words:
                    hit = pointer_words[literal]
                    note = f"literal 0x{literal:08X}"
                    break
        if hit is not None:
            print(
                f"  xref 0x{insn.address:08X}: {insn.mnemonic:<8} {insn.op_str:<28}"
                f" -> {targets[hit]!r} ({note})"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
