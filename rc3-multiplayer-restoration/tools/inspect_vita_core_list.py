import struct
import sys
from pathlib import Path

sys.path.insert(0, r"tools\vita-parse-core")
from core import CoreParser


core = CoreParser(sys.argv[1])


def read_u32(address):
    return struct.unpack("<I", core.read_vaddr(address, 4))[0]


for segment in core.segments:
    print(f"segment {segment.vaddr:#010x}-{segment.vaddr + segment.size:#010x}")

head_data = core.read_vaddr(0x8189DDD4, 4)
table_data = core.read_vaddr(0x8189DDE0, 4)
if head_data is None or table_data is None:
    print("The dump does not contain the module-data globals.")
    for address in (0x8A6679A0, 0x8B646B20, 0x82200B80):
        raw = core.read_vaddr(address, 0x80)
        if raw is not None:
            words = " ".join(
                f"{struct.unpack_from('<I', raw, offset)[0]:08x}"
                for offset in range(0, len(raw), 4)
            )
            print(f"{address:#010x}: {words}")
    corrupt_slot = struct.pack("<I", 0x44980000)
    reported = set()
    for segment in core.segments:
        start = 0
        while True:
            offset = segment.data.find(corrupt_slot, start)
            if offset < 0:
                break
            address = segment.vaddr + offset
            if address not in reported:
                reported.add(address)
                print(f"dump contains 0x44980000 at {address:#010x} (candidate node {address - 0x48:#010x})")
            start = offset + 1
    needle = struct.pack("<I", 0x44980000)
    asset_root = Path("mp_level_analysis/multiplayer_vita_v30_ready/level44")
    for path in asset_root.iterdir():
        if not path.is_file():
            continue
        data = path.read_bytes()
        offsets = []
        start = 0
        while len(offsets) < 20:
            offset = data.find(needle, start)
            if offset < 0:
                break
            offsets.append(offset)
            start = offset + 1
        if offsets:
            print(f"asset {path.name}: " + ", ".join(hex(x) for x in offsets))
    raise SystemExit

head = struct.unpack("<I", head_data)[0]
table = struct.unpack("<I", table_data)[0]
print(f"head={head:#010x} table={table:#010x}")

node = head
seen = set()
for index in range(32):
    if node == 0 or node in seen:
        break
    seen.add(node)
    slot = read_u32(node + 0x48)
    raw = core.read_vaddr(node, 0x60)
    words = " ".join(
        f"{struct.unpack_from('<I', raw, offset)[0]:08x}"
        for offset in range(0, len(raw), 4)
    )
    print(f"{index:02d} node={node:#010x} slot={slot:#010x} {words}")
    if not 0 < slot < 0x1000:
        break
    node = read_u32(table + (slot - 1) * 4)
