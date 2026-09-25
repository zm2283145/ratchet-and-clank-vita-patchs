#!/usr/bin/env python3
"""Find direct Thumb-2 references to strings/data in a sectionless Vita ELF.

The executable uses MOVW/MOVT pairs for most absolute addresses.  This helper
scans every halfword in the RX segment so embedded data does not stop a linear
disassembly, reconstructs nearby pairs, and prints context for requested ASCII
strings.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools"))

from capstone import CS_ARCH_ARM, CS_MODE_THUMB, Cs  # noqa: E402
from capstone.arm import ARM_OP_IMM, ARM_OP_REG  # noqa: E402
from elftools.elf.elffile import ELFFile  # noqa: E402


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("needles", nargs="+")
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        load = next(seg for seg in elf.iter_segments() if seg["p_type"] == "PT_LOAD" and seg["p_flags"] & 1)
        file_start = int(load["p_offset"])
        file_end = file_start + int(load["p_filesz"])
        va_start = int(load["p_vaddr"])

    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    code = blob[file_start:file_end]

    wanted: dict[int, str] = {}
    lower_blob = blob.lower()
    for needle in args.needles:
        start = 0
        query = needle.encode().lower()
        while (offset := lower_blob.find(query, start)) >= 0:
            wanted[va_start + offset - file_start] = blob[offset : blob.find(b"\0", offset)].decode("ascii", "replace")
            start = offset + 1

    refs: dict[int, list[int]] = {address: [] for address in wanted}
    md.skipdata = True
    pending: dict[int, tuple[int, int, int]] = {}
    instruction_index = 0
    for insn in md.disasm(code, va_start):
        instruction_index += 1
        pending = {reg: item for reg, item in pending.items() if instruction_index - item[2] <= 5}
        if insn.mnemonic == "movw" and len(insn.operands) == 2:
            if insn.operands[0].type == ARM_OP_REG and insn.operands[1].type == ARM_OP_IMM:
                pending[insn.operands[0].reg] = (insn.operands[1].imm & 0xFFFF, insn.address, instruction_index)
            continue
        if insn.mnemonic == "movt" and len(insn.operands) == 2:
            reg = insn.operands[0].reg
            if reg in pending and insn.operands[1].type == ARM_OP_IMM:
                low, ref, _ = pending[reg]
                address = ((insn.operands[1].imm & 0xFFFF) << 16) | low
                if address in refs:
                    refs[address].append(ref)
                del pending[reg]

    for target, text in sorted(wanted.items()):
        print(f"\n{target:#010x} {text!r}")
        if not refs[target]:
            print("  (no direct MOVW/MOVT reference found)")
        for ref in refs[target]:
            context_start = max(va_start, ref - 24)
            start_offset = file_start + context_start - va_start
            print(f"  reference {ref:#010x}")
            for insn in md.disasm(blob[start_offset : start_offset + 80], context_start):
                marker = ">" if insn.address == ref else " "
                print(f"   {marker} {insn.address:#010x}: {insn.mnemonic:8} {insn.op_str}")


if __name__ == "__main__":
    main()
