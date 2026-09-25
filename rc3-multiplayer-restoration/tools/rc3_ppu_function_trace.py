#!/usr/bin/env python3
"""Disassemble an RC3 PS3 PPU function and summarize its direct calls.

The decrypted executable is a big-endian PPC64 ELF.  This utility deliberately
uses program headers instead of assuming a fixed file/virtual-address bias, so
it can also be reused with another decrypted EBOOT revision.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from capstone import CS_ARCH_PPC, CS_MODE_64, CS_MODE_BIG_ENDIAN, Cs
from elftools.elf.elffile import ELFFile


def parse_address(value: str) -> int:
    return int(value, 0)


def virtual_slice(elf: ELFFile, address: int, size: int) -> bytes:
    for segment in elf.iter_segments():
        header = segment.header
        start = int(header.p_vaddr)
        end = start + int(header.p_filesz)
        if start <= address and address + size <= end:
            offset = address - start
            return segment.data()[offset : offset + size]
    raise ValueError(f"0x{address:X}..0x{address + size:X} is not file-backed")


def branch_target(instruction) -> int | None:
    if instruction.mnemonic not in {"bl", "bla"}:
        return None
    try:
        return int(instruction.op_str, 0)
    except ValueError:
        return None


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("start", type=parse_address)
    parser.add_argument("end", type=parse_address)
    parser.add_argument("--calls-only", action="store_true")
    args = parser.parse_args()

    if args.end <= args.start:
        parser.error("end must be greater than start")

    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        code = virtual_slice(elf, args.start, args.end - args.start)

    disassembler = Cs(CS_ARCH_PPC, CS_MODE_64 | CS_MODE_BIG_ENDIAN)
    calls: list[tuple[int, int]] = []
    for instruction in disassembler.disasm(code, args.start):
        target = branch_target(instruction)
        if target is not None:
            calls.append((instruction.address, target))
        if not args.calls_only:
            print(
                f"0x{instruction.address:08X}  {instruction.bytes.hex():8}  "
                f"{instruction.mnemonic:9} {instruction.op_str}"
            )

    if args.calls_only:
        for site, target in calls:
            print(f"0x{site:08X} -> 0x{target:08X}")
    else:
        print("\nDirect calls:")
        for site, target in calls:
            print(f"  0x{site:08X} -> 0x{target:08X}")


if __name__ == "__main__":
    main()
