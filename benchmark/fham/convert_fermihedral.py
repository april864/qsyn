#!/usr/bin/env python3
"""
Convert fermihedral model files to qsyn fermionic Hamiltonian (.fham) format.

Mapping rules:
- Input directory (default): ~/fermihedral/model
- Output directory (default): benchmark/fham
- Each non-empty, non-comment line in a fermihedral file is a single term.
- A term is a list of signed integers, e.g.:

    -1 2

    Interpretation:
    - Sign encodes operator type:
      k < 0  → creation operator a_{|k|}^†
      k > 0  → annihilation operator a_{|k|}
  - Indices are the same in both formats (no off-by-one).
  - Coefficient is defaulted to (1, 0) (i.e. 1 + 0j) for every term.

Example:
    fermihedral:  -1 2
    qsyn (.fham): (1, 0) 0^ 1
"""

from __future__ import annotations

import argparse
import pathlib
from typing import Iterable


def parse_line_to_ops(line: str) -> list[tuple[int, bool]]:
    """Parse a single fermihedral term line into (mode, is_creation) ops.

    - Skips comments after '#'.
    - Returns an empty list if there are no operator tokens.
    """
    # Strip comments
    line = line.split("#", maxsplit=1)[0].strip()
    if not line:
        return []

    tokens = line.split()
    ops: list[tuple[int, bool]] = []
    for tok in tokens:
        try:
            k = int(tok)
        except ValueError:
            raise ValueError(f"Invalid integer token '{tok}' in line: {line!r}")

        is_creation = k < 0
        mode = abs(k)  # no index shift; 0 is allowed

        ops.append((mode, is_creation))
    return ops


def convert_file(src_path: pathlib.Path, dst_path: pathlib.Path) -> None:
    """Convert one fermihedral model file to qsyn .fham format."""
    lines: list[str] = src_path.read_text(encoding="utf-8").splitlines()

    # Some fermihedral model files encode Majorana operators using an "mj"
    # token on the first line. We currently do not support Majorana terms, so
    # skip converting such files but emit a clear warning.
    if lines:
        first_tokens = lines[0].split()
        if any(tok == "mj" for tok in first_tokens):
            print(
                f"Warning: skipping {src_path} because it uses Majorana operators "
                "(mj), which are not supported yet."
            )
            return

    out_lines: list[str] = []

    # Many fermihedral model files start with a non-operator header line
    # (e.g., "electronic 10 ac"). We skip the first line unconditionally.
    for idx, line in enumerate(lines):
        if idx == 0:
            continue
        stripped = line.strip()
        # Preserve blank and comment-only lines for readability
        if not stripped or stripped.startswith("#"):
            out_lines.append(stripped)
            continue

        try:
            ops = parse_line_to_ops(line)
        except ValueError as e:
            raise ValueError(f"Error in file {src_path}, line {line!r}: {e}") from e

        if not ops:
            # No operators after stripping comments; skip the line
            continue

        # Default coefficient (1, 0) as requested
        pieces: list[str] = ["(1, 0)"]
        for mode, is_creation in ops:
            pieces.append(f"{mode}{'^' if is_creation else ''}")

        out_lines.append(" ".join(pieces))

    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text("\n".join(out_lines) + "\n", encoding="utf-8")


def iter_model_files(src_dir: pathlib.Path) -> Iterable[pathlib.Path]:
    """Yield all regular files under src_dir (non-recursive)."""
    if not src_dir.exists():
        raise FileNotFoundError(f"Source directory does not exist: {src_dir}")
    for p in sorted(src_dir.iterdir()):
        if p.is_file():
            yield p


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert fermihedral model files to qsyn .fham format."
    )
    parser.add_argument(
        "--src",
        type=pathlib.Path,
        default=pathlib.Path("~").expanduser() / "fermihedral" / "model",
        help="Source directory containing fermihedral model files "
        "(default: ~/fermihedral/model)",
    )
    parser.add_argument(
        "--dst",
        type=pathlib.Path,
        default=pathlib.Path("benchmark") / "fham",
        help="Destination directory for converted .fham files "
        "(default: benchmark/fham)",
    )
    parser.add_argument(
        "--suffix",
        type=str,
        default=".fham",
        help="Suffix to append to converted filenames (default: .fham)",
    )

    args = parser.parse_args()

    src_dir: pathlib.Path = args.src
    dst_dir: pathlib.Path = args.dst
    suffix: str = args.suffix

    for src_file in iter_model_files(src_dir):
        dst_name = src_file.name + suffix if not src_file.name.endswith(suffix) else src_file.name
        dst_path = dst_dir / dst_name
        convert_file(src_file, dst_path)
        print(f"Converted {src_file} -> {dst_path}")


if __name__ == "__main__":
    main()

