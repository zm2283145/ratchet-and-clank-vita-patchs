#!/usr/bin/env python3
"""Find Thumb references and immediate constructions for selected float values."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools"))

from capstone import CS_ARCH_ARM, CS_MODE_THUMB, Cs
from capstone.arm import ARM_OP_IMM, ARM_OP_MEM, ARM_OP_REG, ARM_REG_PC
from elftools.elf.elffile import ELFFile


def show_context(md: Cs, blob: bytes, file_start: int, va_start: int,
                 address: int, label: str) -> None:
    start = max(va_start, (address - 24) & ~1)
    offset = file_start + start - va_start
    print(f"\n{label} at {address:#010x}")
    for insn in md.disasm(blob[offset:offset + 72], start):
        mark = ">" if insn.address == address else " "
        print(f" {mark} {insn.address:#010x}: {insn.mnemonic:<9} {insn.op_str}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("values", nargs="+", type=float)
    parser.add_argument("--start", type=lambda value: int(value, 0))
    parser.add_argument("--end", type=lambda value: int(value, 0))
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        segments = [segment for segment in elf.iter_segments()
                    if segment["p_type"] == "PT_LOAD"]
        rx = next(segment for segment in segments if segment["p_flags"] & 1)

        constants: dict[int, tuple[float, list[int]]] = {}
        for value in args.values:
            bits = struct.unpack("<I", struct.pack("<f", value))[0]
            addresses: list[int] = []
            pattern = struct.pack("<I", bits)
            for segment in segments:
                start = int(segment["p_offset"])
                end = start + int(segment["p_filesz"])
                cursor = start
                while (offset := blob.find(pattern, cursor, end)) >= 0:
                    addresses.append(int(segment["p_vaddr"]) + offset - start)
                    cursor = offset + 1
            constants[bits] = (value, addresses)

    file_start = int(rx["p_offset"])
    file_end = file_start + int(rx["p_filesz"])
    va_start = int(rx["p_vaddr"])
    code = blob[file_start:file_end]
    targets = {address: value for value, addresses in constants.values()
               for address in addresses}

    for bits, (value, addresses) in constants.items():
        print(f"{value:g} ({bits:#010x}) constants: " +
              (", ".join(f"{address:#010x}" for address in addresses) or "none"))

    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    seen: set[tuple[int, str]] = set()
    pending: dict[int, tuple[int, int]] = {}
    instruction_index = 0

    for offset in range(0, len(code) - 4, 2):
        insn = next(md.disasm(code[offset:offset + 4], va_start + offset,
                              count=1), None)
        if insn is None:
            continue
        if args.start is not None and insn.address < args.start:
            continue
        if args.end is not None and insn.address >= args.end:
            continue

        instruction_index += 1
        pending = {reg: item for reg, item in pending.items()
                   if instruction_index - item[1] <= 6}

        if insn.mnemonic == "movw" and len(insn.operands) == 2:
            if (insn.operands[0].type == ARM_OP_REG and
                    insn.operands[1].type == ARM_OP_IMM):
                pending[insn.operands[0].reg] = (
                    insn.operands[1].imm & 0xffff, instruction_index)
        elif insn.mnemonic == "movt" and len(insn.operands) == 2:
            reg = insn.operands[0].reg
            if reg in pending and insn.operands[1].type == ARM_OP_IMM:
                bits = ((insn.operands[1].imm & 0xffff) << 16) | pending[reg][0]
                if bits in constants:
                    label = f"constructs {constants[bits][0]:g}"
                    if (insn.address, label) not in seen:
                        show_context(md, blob, file_start, va_start,
                                     insn.address, label)
                        seen.add((insn.address, label))
        elif insn.mnemonic.startswith("mov"):
            for operand in insn.operands:
                if operand.type == ARM_OP_IMM:
                    bits = operand.imm & 0xffffffff
                    if bits in constants:
                        label = f"immediate {constants[bits][0]:g}"
                        if (insn.address, label) not in seen:
                            show_context(md, blob, file_start, va_start,
                                         insn.address, label)
                            seen.add((insn.address, label))

        for operand in insn.operands:
            if operand.type != ARM_OP_MEM or operand.mem.base != ARM_REG_PC:
                continue
            literal = ((insn.address + 4) & ~3) + operand.mem.disp
            if literal in targets:
                label = f"loads {targets[literal]:g} from {literal:#010x}"
                if (insn.address, label) not in seen:
                    show_context(md, blob, file_start, va_start,
                                 insn.address, label)
                    seen.add((insn.address, label))


if __name__ == "__main__":
    main()
