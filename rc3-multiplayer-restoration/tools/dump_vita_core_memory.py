import struct
import sys

sys.path.insert(0, r"tools\vita-parse-core")
from core import CoreParser


core = CoreParser(sys.argv[1])
address = int(sys.argv[2], 0)
size = int(sys.argv[3], 0)
data = core.read_vaddr(address, size)
if data is None:
    raise SystemExit(f"address range {address:#x}+{size:#x} is not in the dump")

for offset in range(0, len(data), 16):
    row = data[offset:offset + 16]
    words = []
    for word_offset in range(0, len(row), 4):
        word = row[word_offset:word_offset + 4]
        words.append(f"{int.from_bytes(word, 'little'):08x}")
    printable = "".join(chr(value) if 32 <= value < 127 else "." for value in row)
    print(f"{address + offset:08x}  {' '.join(words):35s}  {printable}")

if len(sys.argv) >= 5:
    with open(sys.argv[4], "wb") as output:
        output.write(data)
