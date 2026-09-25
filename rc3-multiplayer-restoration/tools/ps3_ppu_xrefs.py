#!/usr/bin/env python3
"""Locate PowerPC64 references to addresses and floating-point constants.

The RPCS3 executable dumps used by this project are big-endian PS3 ELFs.
Sony's compiler commonly materializes addresses with LIS plus a signed
displacement, or accesses data relative to the executable's TOC in r2.
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
from collections import deque
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".analysis-tools" / "python312"))

from capstone import CS_ARCH_PPC, CS_MODE_64, CS_MODE_BIG_ENDIAN, Cs
from elftools.elf.elffile import ELFFile


MEMORY_RE = re.compile(r"(?P<disp>-?0x[0-9a-f]+|-?[0-9]+)\((?P<reg>r[0-9]+)\)")
REGISTER_RE = re.compile(r"r([0-9]+)")


def number(text: str) -> int:
    return int(text, 0)


def signed_16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def mapped_address(blob: bytes, segments, offset: int) -> int | None:
    for segment in segments:
        start = int(segment["p_offset"])
        end = start + int(segment["p_filesz"])
        if start <= offset < end:
            return int(segment["p_vaddr"]) + offset - start
    return None


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("--address", action="append", default=[], type=number)
    parser.add_argument("--float", dest="floats", action="append", default=[], type=float)
    parser.add_argument("--toc", type=number, help="Static r2 TOC address")
    parser.add_argument("--context", type=int, default=9)
    parser.add_argument("--stores-only", action="store_true",
                        help="Only print store instructions that access a target")
    parser.add_argument("--hits-only", action="store_true",
                        help="Stream matching instructions without retaining full disassembly")
    args = parser.parse_args()

    blob = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        elf = ELFFile(stream)
        segments = [segment for segment in elf.iter_segments()
                    if segment["p_type"] == "PT_LOAD"]
        code_segment = next(segment for segment in segments
                            if int(segment["p_flags"]) & 1)
        executable_sections = [section for section in elf.iter_sections()
                               if section["sh_type"] == "SHT_PROGBITS"
                               and int(section["sh_addr"]) >= int(code_segment["p_vaddr"])
                               and (int(section["sh_addr"]) + int(section["sh_size"])
                                    <= int(code_segment["p_vaddr"]) + int(code_segment["p_filesz"]))]
        code_section = max(executable_sections, key=lambda section: int(section["sh_size"]))

    targets: dict[int, str] = {address: f"address {address:#x}"
                               for address in args.address}
    for value in args.floats:
        pattern = struct.pack(">f", value)
        cursor = 0
        while (offset := blob.find(pattern, cursor)) >= 0:
            address = mapped_address(blob, segments, offset)
            if address is not None:
                targets[address] = f"float {value:g} at {address:#x}"
            cursor = offset + 1

    print("Targets:")
    for address, label in sorted(targets.items()):
        print(f"  {address:#010x}: {label}")

    code_offset = int(code_section["sh_offset"])
    code_address = int(code_section["sh_addr"])
    code = blob[code_offset:code_offset + int(code_section["sh_size"])]
    md = Cs(CS_ARCH_PPC, CS_MODE_64 | CS_MODE_BIG_ENDIAN)
    md.skipdata = True

    if args.hits_only:
        pending: dict[str, tuple[int, int]] = {}
        found = 0
        for index, instruction in enumerate(md.disasm(code, code_address)):
            pending = {reg: state for reg, state in pending.items()
                       if index - state[1] <= 10}
            operands = [item.strip() for item in instruction.op_str.split(",")]
            if instruction.mnemonic == "lis" and len(operands) == 2:
                match = REGISTER_RE.fullmatch(operands[0])
                if match:
                    pending[operands[0]] = (
                        (number(operands[1]) & 0xFFFF) << 16, index)
            elif instruction.mnemonic in {"addi", "addic"} and len(operands) == 3:
                destination, source = operands[0], operands[1]
                if source in pending:
                    pending[destination] = (
                        (pending[source][0] + signed_16(number(operands[2])))
                        & 0xFFFFFFFF, index)
            elif instruction.mnemonic == "ori" and len(operands) == 3:
                destination, source = operands[0], operands[1]
                if source in pending:
                    pending[destination] = (
                        pending[source][0] | (number(operands[2]) & 0xFFFF),
                        index)

            matches: list[int] = []
            for register, (address, _) in pending.items():
                if address in targets and instruction.mnemonic in {"addi", "addic", "ori"}:
                    matches.append(address)
            for match in MEMORY_RE.finditer(instruction.op_str):
                base_register = match.group("reg")
                displacement = number(match.group("disp"))
                if base_register == "r2" and args.toc is not None:
                    address = (args.toc + displacement) & 0xFFFFFFFF
                elif base_register in pending:
                    address = (pending[base_register][0] + displacement) & 0xFFFFFFFF
                else:
                    continue
                if address in targets:
                    matches.append(address)
            if args.stores_only and not instruction.mnemonic.startswith("st"):
                matches = []
            for address in sorted(set(matches)):
                print(f"{instruction.address:#010x}: {instruction.mnemonic:<10} "
                      f"{instruction.op_str} -> {targets[address]}")
                found += 1
        print(f"References found: {found}")
        return

    instructions = list(md.disasm(code, code_address))

    pending: dict[str, tuple[int, int]] = {}
    hits: list[tuple[int, int, str, str]] = []
    for index, instruction in enumerate(instructions):
        pending = {reg: state for reg, state in pending.items()
                   if index - state[1] <= 10}
        operands = [item.strip() for item in instruction.op_str.split(",")]

        if instruction.mnemonic == "lis" and len(operands) == 2:
            match = REGISTER_RE.fullmatch(operands[0])
            if match:
                pending[operands[0]] = ((number(operands[1]) & 0xFFFF) << 16, index)
        elif instruction.mnemonic in {"addi", "addic"} and len(operands) == 3:
            destination, source = operands[0], operands[1]
            if source in pending:
                pending[destination] = ((pending[source][0] + signed_16(number(operands[2])))
                                        & 0xFFFFFFFF, index)
        elif instruction.mnemonic == "ori" and len(operands) == 3:
            destination, source = operands[0], operands[1]
            if source in pending:
                pending[destination] = (pending[source][0] | (number(operands[2]) & 0xFFFF), index)

        for register, (address, _) in pending.items():
            if address in targets and instruction.mnemonic in {"addi", "addic", "ori"}:
                hits.append((index, address, instruction.mnemonic,
                             f"{instruction.op_str} -> {register}={address:#x}"))

        for match in MEMORY_RE.finditer(instruction.op_str):
            base_register = match.group("reg")
            displacement = number(match.group("disp"))
            if base_register == "r2" and args.toc is not None:
                address = (args.toc + displacement) & 0xFFFFFFFF
            elif base_register in pending:
                address = (pending[base_register][0] + displacement) & 0xFFFFFFFF
            else:
                continue
            if address in targets:
                hits.append((index, address, instruction.mnemonic, instruction.op_str))

    if args.stores_only:
        hits = [hit for hit in hits if hit[2].startswith("st")]

    for index, target, _, _ in hits:
        instruction = instructions[index]
        print(f"\nReference to {targets[target]} at {instruction.address:#010x}")
        start = max(0, index - args.context)
        end = min(len(instructions), index + args.context + 1)
        for current in instructions[start:end]:
            marker = ">" if current.address == instruction.address else " "
            print(f" {marker} {current.address:#010x}: {current.mnemonic:<10} {current.op_str}")

    print(f"\nReferences found: {len(hits)}")


if __name__ == "__main__":
    main()
