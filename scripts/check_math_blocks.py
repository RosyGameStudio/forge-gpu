"""Check that $$ display math blocks use multi-line syntax.

GitHub renders $$...$$ on a single line as inline math, not display math.
The $$ delimiters must be on their own lines for block rendering:

    Good:                Bad:
    $$                   $$x^2 + y^2 = 1$$
    x^2 + y^2 = 1
    $$
"""

import re
import sys
from pathlib import Path

# Match lines where $$ opens AND closes on the same line with content between
SINGLE_LINE_DISPLAY = re.compile(r"^\$\$\s*\S.*\$\$\s*$")

IGNORE_DIRS = {"node_modules", "build", "third_party", ".git"}


def find_markdown_files(root: Path) -> list[Path]:
    """Find all .md files, skipping ignored directories."""
    results = []
    for path in root.rglob("*.md"):
        if any(part in IGNORE_DIRS for part in path.parts):
            continue
        results.append(path)
    return sorted(results)


def check_file(path: Path) -> list[tuple[int, str]]:
    """Return list of (line_number, line_text) for single-line $$ blocks."""
    errors = []
    for i, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if SINGLE_LINE_DISPLAY.match(line):
            errors.append((i, line))
    return errors


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    files = find_markdown_files(root)
    total_errors = 0

    for path in files:
        errors = check_file(path)
        for lineno, line in errors:
            rel = path.relative_to(root)
            print(f"{rel}:{lineno}: single-line $$ block: {line.strip()}")
            total_errors += 1

    if total_errors:
        print(f"\n{total_errors} display math block(s) need multi-line syntax.")
        print("Split $$...$$ onto three lines:")
        print("  $$")
        print("  <formula>")
        print("  $$")
        return 1

    print("All display math blocks use multi-line syntax.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
