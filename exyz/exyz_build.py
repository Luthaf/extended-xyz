import os
from cffi import FFI

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

builder = FFI()

with open(os.path.join(ROOT, "src/exyz.h")) as fd:
    lines = fd.readlines()
    header = "\n".join(lines[8:-4])

    header += "\nvoid free(void*);"

builder.cdef(header)

builder.set_source(
    "exyz._exyz",
    f'#include "{os.path.join(ROOT, "src/exyz.h")}"',
    sources=["src/types.c", "src/parser.c", "src/writer.c"],
)

if __name__ == "__main__":
    builder.compile(verbose=True, debug=True)
