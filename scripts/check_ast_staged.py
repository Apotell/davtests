#!/usr/bin/env python3
"""
Report staged files that contain changes inside AST_DEBUG_BEGIN / AST_DEBUG_END blocks.

Exit code: 0 if no hits, 1 if any file has changes inside an AST block.
"""

import multiprocessing
import os
import re
import subprocess
import sys


def run(args):
    return subprocess.run(args, capture_output=True, text=True, check=True)


def get_staged_files():
    out = run(["git", "diff", "--cached", "--name-only", "--diff-filter=ACMRD"]).stdout
    return [f for f in out.splitlines() if f]


def file_lines(ref_colon_path):
    result = subprocess.run(["git", "show", ref_colon_path], capture_output=True, text=True)
    return result.stdout.splitlines() if result.returncode == 0 else []


def ast_block_line_set(lines):
    """Return a set of 1-indexed line numbers that lie inside AST_DEBUG_BEGIN/END blocks."""
    inside = set()
    depth = 0
    for i, line in enumerate(lines, 1):
        if "AST_DEBUG_BEGIN" in line:
            depth += 1
        if depth > 0:
            inside.add(i)
        if "AST_DEBUG_END" in line and depth > 0:
            depth -= 1
    return inside


def diff_line_sets(path):
    """Return (old_lines, new_lines) as sets of 1-indexed line numbers affected by the staged diff."""
    diff = run(["git", "diff", "--cached", "-U0", "--", path]).stdout
    old_lines, new_lines = set(), set()
    for m in re.finditer(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", diff, re.MULTILINE):
        def expand(start_str, count_str):
            count = int(count_str) if count_str is not None else 1
            return set(range(int(start_str), int(start_str) + count))
        old_lines |= expand(m.group(1), m.group(2))
        new_lines |= expand(m.group(3), m.group(4))
    return old_lines, new_lines


def check_file(path):
    """Return path if it has staged changes inside an AST block, else None."""
    old_ranges, new_ranges = diff_line_sets(path)

    if new_ranges and ast_block_line_set(file_lines(f":{path}")) & new_ranges:
        return path

    if old_ranges and ast_block_line_set(file_lines(f"HEAD:{path}")) & old_ranges:
        return path

    return None


def main():
    staged = get_staged_files()
    if not staged:
        print("No staged files.")
        return 0

    workers = min(len(staged), os.cpu_count() or 4)
    with multiprocessing.Pool(workers) as pool:
        results = pool.map(check_file, staged)

    hits = sorted(path for path in results if path is not None)

    if hits:
        print(f"Files with staged changes inside AST_DEBUG_BEGIN/END blocks ({len(hits)}):")
        for f in hits:
            print(f"  {f}")
    else:
        print("No staged changes inside AST_DEBUG_BEGIN/END blocks.")

    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
