#!/usr/bin/env python3
"""Layout-preserving RC3 PS3 -> Vita gameplay bundle converter.

This is deliberately limited to gameplay data.  Engine geometry, Vita's
separate engine_vert bundle, and VRAM textures are different conversion
problems and are not emitted by this tool.

The source bundle is retained as the layout authority.  Known numeric
sections can be overlaid from a little-endian Replanetizer serialization,
while language records are converted in place so their original strings and
padding survive unchanged.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, asdict
from pathlib import Path


HEADER_SIZE = 0xA0
FIELD_NAMES = (
    "levelVar", "lights", "camera", "sound",
    "english", "ukenglish", "french", "german",
    "spanish", "italian", "japanese", "korean",
    "tieId", "tie", "tieGroups", "shrubId",
    "shrub", "shrubGroups", "mobyId", "moby",
    "mobyGroups", "globalPvar", "pvarScratch", "pvarSize",
    "pvar", "pvarRewire", "cuboid", "sphere",
    "cylinder", "pill", "spline", "grindPaths",
    "pointLight", "envTransitions", "camCollision", "envSamples",
    "occlusion", "tieAmbient", "areas",
)
LANGUAGE_FIELDS = {
    "english", "ukenglish", "french", "german", "spanish", "italian",
    "japanese", "korean",
}

FIXED_RECORD_SIZES = {
    "lights": 0x40,
    "camera": 0x20,
    "sound": 0x90,
    "moby": 0x88,
    "cuboid": 0x80,
    "sphere": 0x80,
    "cylinder": 0x80,
    "pill": 0x80,
    "grindPaths": 0x20,
    "pointLight": 0x10,
    "envTransitions": 0x80,
    "areas": 0x30,
}

# Runtime-verified multiplayer pvar fields that the campaign-trained size
# model cannot classify safely.  Model 4202's 928-byte pvar contains gameplay
# object indices at these offsets; leaving them in PS3 byte order produced the
# level-44 crash at Vita 0x8116B7BC (0x29 became 0x29000000).
PVAR_OWNER_FIELD_OVERRIDES = {
    (4202, 928, 0x34): "reverse32",
    (4202, 928, 0x44): "reverse32",
    (4202, 928, 0x60): "reverse32",
    (4202, 928, 0x64): "reverse32",
}


class ConversionError(RuntimeError):
    pass


@dataclass
class SectionResult:
    name: str
    source_offset: int
    source_size: int
    action: str
    typed_offset: int | None = None
    typed_size: int | None = None
    bytes_written: int = 0


def unpack_u32(data: bytes | bytearray, offset: int, endian: str) -> int:
    return struct.unpack_from(endian + "I", data, offset)[0]


def pack_u32(data: bytearray, offset: int, value: int, endian: str) -> None:
    struct.pack_into(endian + "I", data, offset, value)


def parse_header(data: bytes | bytearray, endian: str) -> dict[str, int]:
    if len(data) < HEADER_SIZE:
        raise ConversionError(f"bundle is smaller than the 0x{HEADER_SIZE:X}-byte header")
    return {
        name: unpack_u32(data, index * 4, endian)
        for index, name in enumerate(FIELD_NAMES)
    }


def validate_header(header: dict[str, int], length: int, label: str) -> None:
    bad = [(name, value) for name, value in header.items() if value and not (HEADER_SIZE <= value < length)]
    if bad:
        rendered = ", ".join(f"{name}=0x{value:X}" for name, value in bad[:6])
        raise ConversionError(f"{label} has invalid section pointers: {rendered}")


def section_ranges(header: dict[str, int], length: int) -> dict[str, tuple[int, int]]:
    starts = sorted({value for value in header.values() if value})
    next_start = {start: (starts[i + 1] if i + 1 < len(starts) else length) for i, start in enumerate(starts)}
    return {
        name: (start, next_start[start])
        for name, start in header.items()
        if start
    }


def convert_language_in_place(output: bytearray, start: int, end: int) -> int:
    """Endian-convert a LanguageData header/table but preserve string bytes."""
    size = end - start
    if size < 8:
        raise ConversionError(f"language section at 0x{start:X} is only {size} bytes")

    count = unpack_u32(output, start, ">")
    declared_size = unpack_u32(output, start + 4, ">")
    table_end = 8 + count * 0x10
    if count > 0x100000 or table_end > size:
        raise ConversionError(
            f"invalid language table at 0x{start:X}: count={count}, section size={size}"
        )
    if declared_size and declared_size > size:
        raise ConversionError(
            f"invalid language size at 0x{start:X}: declared={declared_size}, section={size}"
        )

    # The two section-header words and first three words in every record are
    # u32 values.  The fourth record word contains two u16 values (often
    # FF FF and 00 00, which initially made it look like padding).  String and
    # alignment bytes following the table are intentionally untouched.
    for relative in (0, 4):
        value = unpack_u32(output, start + relative, ">")
        pack_u32(output, start + relative, value, "<")
    for index in range(count):
        record = start + 8 + index * 0x10
        for relative in (0, 4, 8):
            value = unpack_u32(output, record + relative, ">")
            pack_u32(output, record + relative, value, "<")
        for relative in (12, 14):
            value = struct.unpack_from(">H", output, record + relative)[0]
            struct.pack_into("<H", output, record + relative, value)
    return table_end


def sha256(data: bytes | bytearray) -> str:
    return hashlib.sha256(data).hexdigest()


def word_mode(source: bytes, target: bytes) -> str | None:
    """Return an unambiguous byte-order operation for one four-byte word."""
    candidates = {
        "same": source,
        "reverse32": source[::-1],
        "swap16": source[1::-1] + source[3:1:-1],
    }
    matches = [name for name, value in candidates.items() if value == target]
    return matches[0] if len(matches) == 1 else None


def transform_word(word: bytes, mode: str) -> bytes:
    if mode == "same":
        return word
    if mode == "reverse32":
        return word[::-1]
    if mode == "swap16":
        return word[1::-1] + word[3:1:-1]
    raise ConversionError(f"unknown word transform {mode}")


def pvar_entries(data: bytes, endian: str) -> list[tuple[int, int]]:
    header = parse_header(data, endian)
    size_start = header["pvarSize"]
    data_start = header["pvar"]
    if not size_start or not data_start or data_start <= size_start:
        return []
    data_end = header["pvarRewire"] or len(data)
    total = max(0, data_end - data_start)
    entries: list[tuple[int, int]] = []
    for pos in range(size_start, data_start - 7, 8):
        offset = unpack_u32(data, pos, endian)
        size = unpack_u32(data, pos + 4, endian)
        if size == 0 and offset == 0 and entries:
            continue
        if offset + size <= total:
            entries.append((offset, size))
    return entries


def pvar_owners(data: bytes, endian: str) -> dict[int, set[int]]:
    """Map pvar indices to RC3 moby model/oClass IDs."""
    header = parse_header(data, endian)
    start = header["moby"]
    owners: dict[int, set[int]] = defaultdict(set)
    if not start or start + 0x10 > len(data):
        return owners
    count = unpack_u32(data, start, endian)
    records = start + 0x10
    for index in range(count):
        pos = records + index * 0x88
        if pos + 0x88 > len(data):
            break
        model_id = unpack_u32(data, pos + 0x28, endian)
        pvar_index = unpack_u32(data, pos + 0x68, endian)
        if pvar_index != 0xFFFFFFFF:
            owners[pvar_index].add(model_id)

    # Cameras and sounds also own pvars but do not have moby model IDs.  Use
    # stable negative pseudo-IDs so their schemas can still be learned.
    for section, element_size, pvar_relative, pseudo_id in (
        ("camera", 0x20, 0x1C, -2),
        ("sound", 0x90, 0x08, -3),
    ):
        start = header[section]
        if not start or start + 0x10 > len(data):
            continue
        item_count = unpack_u32(data, start, endian)
        for index in range(item_count):
            pos = start + 0x10 + index * element_size
            if pos + element_size > len(data):
                break
            pvar_index = unpack_u32(data, pos + pvar_relative, endian)
            if pvar_index != 0xFFFFFFFF:
                owners[pvar_index].add(pseudo_id)
    return owners


def choose_modes(votes: dict[tuple[object, ...], Counter]) -> dict[tuple[object, ...], str]:
    chosen: dict[tuple[object, ...], str] = {}
    for key, counts in votes.items():
        ranked = counts.most_common()
        if ranked and (len(ranked) == 1 or ranked[0][1] >= ranked[1][1] * 4):
            chosen[key] = ranked[0][0]
    return chosen


def train_word_model(ps3_root: Path, vita_root: Path) -> dict[str, object]:
    ps3_files = {p.parent.name: p for p in ps3_root.rglob("gameplay_ntsc")}
    vita_files = {p.parent.name: p for p in vita_root.rglob("gameplay_ntsc")}
    levels = sorted(set(ps3_files) & set(vita_files))
    if not levels:
        raise ConversionError("training roots contain no matching gameplay_ntsc level pairs")

    fixed_votes: dict[tuple[object, ...], Counter] = defaultdict(Counter)
    fixed_exact_votes: dict[tuple[object, ...], Counter] = defaultdict(Counter)
    pvar_owner_votes: dict[tuple[object, ...], Counter] = defaultdict(Counter)
    pvar_size_votes: dict[tuple[object, ...], Counter] = defaultdict(Counter)
    pvar_owner_exact_votes: dict[tuple[object, ...], Counter] = defaultdict(Counter)
    pvar_size_exact_votes: dict[tuple[object, ...], Counter] = defaultdict(Counter)

    for level in levels:
        source = ps3_files[level].read_bytes()
        target = vita_files[level].read_bytes()
        if len(source) != len(target):
            continue
        source_header = parse_header(source, ">")
        target_header = parse_header(target, "<")
        source_ranges = section_ranges(source_header, len(source))
        target_ranges = section_ranges(target_header, len(target))

        for name, element_size in FIXED_RECORD_SIZES.items():
            if name not in source_ranges or name not in target_ranges:
                continue
            s0, s1 = source_ranges[name]
            t0, t1 = target_ranges[name]
            size = min(s1 - s0, t1 - t0)
            for relative in range(0, size - 3, 4):
                source_word = source[s0 + relative:s0 + relative + 4]
                target_word = target[t0 + relative:t0 + relative + 4]
                mode = word_mode(source_word, target_word)
                key = (
                    name,
                    "header" if relative < 0x10 else "record",
                    relative if relative < 0x10 else (relative - 0x10) % element_size,
                )
                if mode is not None:
                    fixed_votes[key][mode] += 1
                exact_mode = "same" if source_word == target_word else mode
                if exact_mode is not None:
                    fixed_exact_votes[key + (source_word,)][exact_mode] += 1

        entries = pvar_entries(source, ">")
        target_entries = pvar_entries(target, "<")
        owners = pvar_owners(source, ">")
        source_pvar = source_header["pvar"]
        target_pvar = target_header["pvar"]
        for pvar_index, ((source_offset, size), (target_offset, target_size)) in enumerate(
            zip(entries, target_entries)
        ):
            if size != target_size:
                continue
            ids = owners.get(pvar_index, set())
            for relative in range(0, size - 3, 4):
                source_word = source[
                    source_pvar + source_offset + relative:source_pvar + source_offset + relative + 4
                ]
                target_word = target[
                    target_pvar + target_offset + relative:target_pvar + target_offset + relative + 4
                ]
                mode = word_mode(source_word, target_word)
                exact_mode = "same" if source_word == target_word else mode
                if mode is not None:
                    pvar_size_votes[(size, relative)][mode] += 1
                for model_id in ids:
                    if mode is not None:
                        pvar_owner_votes[(model_id, size, relative)][mode] += 1
                if exact_mode is not None:
                    pvar_size_exact_votes[(size, relative, source_word)][exact_mode] += 1
                    for model_id in ids:
                        pvar_owner_exact_votes[
                            (model_id, size, relative, source_word)
                        ][exact_mode] += 1

    return {
        "levels": levels,
        "fixed": choose_modes(fixed_votes),
        "fixed_exact": choose_modes(fixed_exact_votes),
        "pvar_owner": choose_modes(pvar_owner_votes),
        "pvar_size": choose_modes(pvar_size_votes),
        "pvar_owner_exact": choose_modes(pvar_owner_exact_votes),
        "pvar_size_exact": choose_modes(pvar_size_exact_votes),
    }


def apply_word_model(
    output: bytearray,
    source: bytes,
    model: dict[str, object],
) -> dict[str, int]:
    header = parse_header(source, ">")
    ranges = section_ranges(header, len(source))
    fixed: dict[tuple[object, ...], str] = model["fixed"]
    fixed_exact: dict[tuple[object, ...], str] = model["fixed_exact"]
    applied_fixed = 0
    for name, element_size in FIXED_RECORD_SIZES.items():
        if name not in ranges:
            continue
        start, end = ranges[name]
        for relative in range(0, end - start - 3, 4):
            key = (
                name,
                "header" if relative < 0x10 else "record",
                relative if relative < 0x10 else (relative - 0x10) % element_size,
            )
            pos = start + relative
            source_word = source[pos:pos + 4]
            mode = fixed_exact.get(key + (source_word,)) or fixed.get(key) or "same"
            output[pos:pos + 4] = transform_word(source[pos:pos + 4], mode)
            applied_fixed += 1

    owner_modes: dict[tuple[object, ...], str] = model["pvar_owner"]
    size_modes: dict[tuple[object, ...], str] = model["pvar_size"]
    owner_exact: dict[tuple[object, ...], str] = model["pvar_owner_exact"]
    size_exact: dict[tuple[object, ...], str] = model["pvar_size_exact"]
    entries = pvar_entries(source, ">")
    owners = pvar_owners(source, ">")
    pvar_start = header["pvar"]
    applied_override = applied_owner = applied_size = fallback = 0
    for index, (offset, size) in enumerate(entries):
        ids = owners.get(index, set())
        for relative in range(0, size - 3, 4):
            pos = pvar_start + offset + relative
            source_word = source[pos:pos + 4]
            exact_owner_choices = {
                owner_exact[(model_id, size, relative, source_word)]
                for model_id in ids
                if (model_id, size, relative, source_word) in owner_exact
            }
            owner_choices = {
                owner_modes[(model_id, size, relative)]
                for model_id in ids
                if (model_id, size, relative) in owner_modes
            }
            override_choices = {
                PVAR_OWNER_FIELD_OVERRIDES[(model_id, size, relative)]
                for model_id in ids
                if (model_id, size, relative) in PVAR_OWNER_FIELD_OVERRIDES
            }
            if len(override_choices) == 1:
                mode = next(iter(override_choices))
                applied_override += 1
            elif len(exact_owner_choices) == 1:
                mode = next(iter(exact_owner_choices))
                applied_owner += 1
            elif (size, relative, source_word) in size_exact:
                mode = size_exact[(size, relative, source_word)]
                applied_size += 1
            elif len(owner_choices) == 1:
                mode = next(iter(owner_choices))
                applied_owner += 1
            elif (size, relative) in size_modes:
                mode = size_modes[(size, relative)]
                applied_size += 1
            else:
                mode = "reverse32"
                fallback += 1
            output[pos:pos + 4] = transform_word(source[pos:pos + 4], mode)

    # Group sections combine endian-sensitive offset tables with opaque ID
    # payloads.  Replanetizer currently preserves these blocks verbatim.
    for name in ("tieGroups", "shrubGroups"):
        if name not in ranges:
            continue
        start, end = ranges[name]
        count = unpack_u32(source, start, ">")
        length = unpack_u32(source, start + 4, ">")
        for relative in range(0, min(0x10 + count * 4, end - start), 4):
            pos = start + relative
            output[pos:pos + 4] = source[pos:pos + 4][::-1]
        payload = start + 0x10 + count * 4
        for pos in range(payload, min(payload + length, end) - 1, 2):
            output[pos:pos + 2] = source[pos:pos + 2][::-1]

    if "mobyGroups" in ranges:
        start, end = ranges["mobyGroups"]
        count = unpack_u32(source, start, ">")
        for relative in range(0, min(0x10 + count * 4, end - start), 4):
            pos = start + relative
            output[pos:pos + 4] = source[pos:pos + 4][::-1]

    if "camCollision" in ranges:
        start, end = ranges["camCollision"]
        for pos in range(start, min(start + 0x10, end), 4):
            output[pos:pos + 4] = source[pos:pos + 4][::-1]
    return {
        "fixed_words": applied_fixed,
        "pvar_override_words": applied_override,
        "pvar_owner_words": applied_owner,
        "pvar_size_words": applied_size,
        "pvar_fallback_words": fallback,
    }


def comparison_stats(actual: bytes, reference: bytes) -> dict[str, object]:
    common = min(len(actual), len(reference))
    common_differences = sum(a != b for a, b in zip(actual[:common], reference[:common]))
    differing = common_differences + abs(len(actual) - len(reference))
    first = next((i for i in range(common) if actual[i] != reference[i]), None)
    if first is None and len(actual) != len(reference):
        first = common
    return {
        "reference_length": len(reference),
        "output_length": len(actual),
        "differing_bytes": differing,
        "exact_bytes_in_common": common - common_differences,
        "exact_percent_in_common": round(
            100.0 * (common - common_differences) / common,
            4,
        ) if common else 100.0,
        "first_difference": first,
        "exact_match": actual == reference,
    }


def section_comparison_stats(
    actual: bytes,
    reference: bytes,
    header: dict[str, int],
) -> list[dict[str, object]]:
    """Report byte differences using the converted file's preserved layout."""
    ranges = section_ranges(header, min(len(actual), len(reference)))
    results: list[dict[str, object]] = []
    for name in FIELD_NAMES:
        if name not in ranges:
            continue
        start, end = ranges[name]
        stats = comparison_stats(actual[start:end], reference[start:end])
        results.append({
            "name": name,
            "offset": start,
            "size": end - start,
            "differing_bytes": stats["differing_bytes"],
            "first_difference": (
                start + stats["first_difference"]
                if stats["first_difference"] is not None else None
            ),
            "exact_match": stats["exact_match"],
        })
    return results


def convert(
    source: bytes,
    typed: bytes | None,
    near_delta: int,
) -> tuple[bytes, list[SectionResult]]:
    source_header = parse_header(source, ">")
    validate_header(source_header, len(source), "PS3 source")
    source_ranges = section_ranges(source_header, len(source))
    output = bytearray(source)
    results: list[SectionResult] = []

    typed_header: dict[str, int] = {}
    typed_ranges: dict[str, tuple[int, int]] = {}
    if typed is not None:
        typed_header = parse_header(typed, "<")
        validate_header(typed_header, len(typed), "typed little-endian input")
        typed_ranges = section_ranges(typed_header, len(typed))

    # The Vita file keeps the PS3 section addresses when layout is preserved.
    for index, name in enumerate(FIELD_NAMES):
        pack_u32(output, index * 4, source_header[name], "<")
    # Preserve and endian-convert the final unknown/reserved header word too.
    pack_u32(output, 0x9C, unpack_u32(source, 0x9C, ">"), "<")

    for name in FIELD_NAMES:
        if name not in source_ranges:
            continue
        source_start, source_end = source_ranges[name]
        source_size = source_end - source_start

        if name in LANGUAGE_FIELDS:
            written = convert_language_in_place(output, source_start, source_end)
            results.append(SectionResult(name, source_start, source_size, "language-table", bytes_written=written))
            continue

        if typed is None or name not in typed_ranges:
            results.append(SectionResult(name, source_start, source_size, "preserved"))
            continue

        typed_start, typed_end = typed_ranges[name]
        typed_size = typed_end - typed_start
        difference = abs(source_size - typed_size)
        if source_size == typed_size:
            output[source_start:source_end] = typed[typed_start:typed_end]
            results.append(SectionResult(
                name, source_start, source_size, "typed-exact-size",
                typed_start, typed_size, source_size,
            ))
        elif difference <= near_delta:
            amount = min(source_size, typed_size)
            output[source_start:source_start + amount] = typed[typed_start:typed_start + amount]
            results.append(SectionResult(
                name, source_start, source_size, "typed-preserve-tail",
                typed_start, typed_size, amount,
            ))
        else:
            results.append(SectionResult(
                name, source_start, source_size, "preserved-size-mismatch",
                typed_start, typed_size, 0,
            ))

    # RC3's one-chunk level-variable block is mixed-endian even on PS3: its
    # final OFF_7C and OFF_80 words are already stored little-endian.  The Vita
    # files preserve those eight bytes verbatim.  A normal big-to-little typed
    # serialization therefore swaps them incorrectly, so restore the source
    # bytes after the typed overlay.
    if "levelVar" in source_ranges:
        level_start, level_end = source_ranges["levelVar"]
        if level_end - level_start == 0x84:
            output[level_start + 0x7C:level_start + 0x84] = \
                source[level_start + 0x7C:level_start + 0x84]

    return bytes(output), results


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Convert an RC3 PS3 gameplay bundle toward the Vita layout without repacking it."
    )
    parser.add_argument("source", type=Path, help="PS3 gameplay_ntsc/gameplay_pal input")
    parser.add_argument("output", type=Path, help="converted gameplay output")
    parser.add_argument(
        "--typed-gameplay", type=Path,
        help="little-endian gameplay emitted by the patched Replanetizer serializer",
    )
    parser.add_argument(
        "--near-delta", type=int, default=16,
        help="overlay typed sections whose length differs by at most this many bytes (default: 16)",
    )
    parser.add_argument(
        "--training-ps3-root", type=Path,
        help="root containing paired PS3 gameplay_ntsc files used to learn mixed-endian fields",
    )
    parser.add_argument(
        "--training-vita-root", type=Path,
        help="root containing paired Vita gameplay_ntsc files used to learn mixed-endian fields",
    )
    parser.add_argument("--reference-vita", type=Path, help="known Vita file for validation")
    parser.add_argument("--report", type=Path, help="write a JSON conversion report")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        source = args.source.read_bytes()
        typed = args.typed_gameplay.read_bytes() if args.typed_gameplay else None
        converted, sections = convert(source, typed, args.near_delta)
        word_model = None
        model_stats = None
        if args.training_ps3_root or args.training_vita_root:
            if not args.training_ps3_root or not args.training_vita_root:
                raise ConversionError("both training roots must be supplied together")
            word_model = train_word_model(args.training_ps3_root, args.training_vita_root)
            converted_buffer = bytearray(converted)
            model_stats = apply_word_model(converted_buffer, source, word_model)
            converted = bytes(converted_buffer)
    except (OSError, ConversionError, struct.error) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(converted)
    report: dict[str, object] = {
        "status": "prototype-gameplay-only",
        "source": str(args.source.resolve()),
        "output": str(args.output.resolve()),
        "typed_gameplay": str(args.typed_gameplay.resolve()) if args.typed_gameplay else None,
        "source_length": len(source),
        "output_length": len(converted),
        "source_sha256": sha256(source),
        "output_sha256": sha256(converted),
        "near_delta": args.near_delta,
        "sections": [asdict(item) for item in sections],
        "limitations": [
            "Does not convert engine.ps3 or generate engine_vert.ps3.",
            "Does not convert vram.ps3 or mobyload bundles.",
            "A non-matching validation result is not hardware-ready.",
        ],
    }
    if word_model is not None:
        report["word_model"] = {
            "training_levels": word_model["levels"],
            "fixed_rules": len(word_model["fixed"]),
            "pvar_owner_rules": len(word_model["pvar_owner"]),
            "pvar_size_rules": len(word_model["pvar_size"]),
            "applied": model_stats,
        }
    if args.reference_vita:
        reference = args.reference_vita.read_bytes()
        report["validation"] = comparison_stats(converted, reference)
        source_header = parse_header(source, ">")
        report["validation_by_section"] = section_comparison_stats(
            converted, reference, source_header,
        )

    report_path = args.report or args.output.with_suffix(args.output.suffix + ".json")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print(f"Wrote {args.output} ({len(converted):,} bytes)")
    print(f"Report: {report_path}")
    if "validation" in report:
        validation = report["validation"]
        print(
            f"Validation: {validation['exact_percent_in_common']:.4f}% exact; "
            f"{validation['differing_bytes']:,} differing bytes"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
