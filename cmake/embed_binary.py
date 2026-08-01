#!/usr/bin/env python3
"""Embed a binary file as a C++ byte array."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import tempfile


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--symbol", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", args.symbol) is None:
        raise ValueError(f"invalid C++ symbol: {args.symbol!r}")

    data = args.input.read_bytes()
    args.output.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="ascii",
        newline="\n",
        dir=args.output.parent,
        prefix=f".{args.output.name}.",
        delete=False,
    ) as output:
        temporary = Path(output.name)
        output.write(f"unsigned char {args.symbol}[] = {{\n")
        for offset in range(0, len(data), 12):
            chunk = data[offset : offset + 12]
            output.write("  ")
            output.write(", ".join(f"0x{value:02x}" for value in chunk))
            if offset + len(chunk) < len(data):
                output.write(",")
            output.write("\n")
        output.write("};\n")
        output.write(f"unsigned int {args.symbol}_len = {len(data)};\n")

    os.replace(temporary, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
