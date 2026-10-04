import pathlib
import struct
import sys

source = pathlib.Path(sys.argv[1])
destination = pathlib.Path(sys.argv[2])
image = bytearray(source.read_bytes())

if len(image) < 64 or image[:4] != b"\x7fELF":
    raise SystemExit("source is not a complete ELF64 image")

mutation = sys.argv[3]
if mutation == "program-header-bounds":
    struct.pack_into("<Q", image, 32, len(image) - 1)
elif mutation == "writable-executable":
    program_header_offset = struct.unpack_from("<Q", image, 32)[0]
    struct.pack_into("<I", image, program_header_offset + 4, 7)
elif mutation == "entry-point":
    struct.pack_into("<Q", image, 24, 0x0000000200000000)
else:
    raise SystemExit(f"unknown image mutation: {mutation}")

destination.write_bytes(image)
