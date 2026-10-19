#!/usr/bin/env python3

import argparse
import sys
from pathlib import Path
from re import Pattern

from utils import build_filters

_this_filepath = Path(__file__).resolve()
_workspace_dirpath = _this_filepath.parent.parent
_default_test_dirpath = _workspace_dirpath / 'hlc'


def _dumb_hash(name: str) -> int:
    return sum(ord(c) for c in name)


def _is_filtered(key: str, filters: list[str | Pattern]) -> bool:
    if not filters:
        return True
    for filt in filters:
        if isinstance(filt, str):
            if filt.lower() == key.lower():
                return True
        elif filt.search(key):
            return True
    return False


def _find_owners(test_dirpath: Path) -> list[Path]:
    # Folders below test_dirpath with their own CMakeLists.txt, topmost only:
    # like cmake/DavTests.cmake, the walk stops at the first one, which then
    # builds every test_*.cpp below it (possibly into one bulk executable).
    #
    # Every test_*.cpp below such a folder is treated as one unit, so the whole
    # folder lands in a single shard and a bulk executable is never split
    # across CI jobs. This script only writes the list of files for a shard;
    # it does not decide between bulk and one executable per file. That is set
    # by the folder's CMakeLists.txt and done by CMake, which runs after this
    # script.
    owners = sorted({p.parent for p in test_dirpath.rglob('CMakeLists.txt') if p.parent != test_dirpath})
    return [o for o in owners if not any(other in o.parents for other in owners)]


def _owner_of(src: Path, owners: list[Path]) -> Path | None:
    for owner in owners:
        if owner in src.parents:
            return owner
    return None


def _pack(units: dict[str, list[Path]], shard: int, num_shards: int) -> list[Path]:
    # Greedy longest-first packing: each unit goes whole to the currently
    # smallest shard. The result depends only on unit sizes and names, so every
    # shard job computes the same assignment.
    loads = [0] * num_shards
    selected = []
    for key in sorted(units, key=lambda k: (-len(units[k]), k)):
        target = min(range(num_shards), key=lambda i: (loads[i], i))
        loads[target] += len(units[key])
        if target == shard:
            selected.extend(units[key])
    return sorted(selected, key=lambda p: p.as_posix().lower())


def _scan(test_dirpath: Path, filters: list[str | Pattern], shard: int, num_shards: int) -> tuple[list[Path], int]:
    all_sources = sorted(test_dirpath.rglob('test_*.cpp'), key=lambda p: p.as_posix().lower())
    filtered = [src for src in all_sources
                if _is_filtered(src.parent.relative_to(test_dirpath).as_posix(), filters)]

    owners = _find_owners(test_dirpath)
    units: dict[str, list[Path]] = {}
    for src in filtered:
        owner = _owner_of(src, owners)
        if owner is not None:
            key = 'folder:' + owner.relative_to(test_dirpath).as_posix()
        else:
            key = 'dir:' + src.parent.relative_to(test_dirpath).as_posix()
        units.setdefault(key, []).append(src)

    if not any(key.startswith('folder:') for key in units):
        # No per-folder CMakeLists.txt involved: shard by directory hash as before.
        selected = [
            src for src in filtered
            if (_dumb_hash(src.parent.relative_to(test_dirpath).as_posix()) % num_shards) == shard
        ]
        return selected, len(all_sources)

    # A folder with its own CMakeLists.txt is one unit, kept whole in one shard,
    # so each bulk executable is built and run by exactly one shard job.
    return _pack(units, shard, num_shards), len(all_sources)


def _main() -> int:
    parser = argparse.ArgumentParser(
        description='Generate a shard-filtered list of test_*.cpp paths for CMake.')
    parser.add_argument(
        '--test-dirpath', dest='test_dirpath', required=False,
        default=_default_test_dirpath, type=Path,
        help='Root directory to scan for test_*.cpp files (default: hlc/)')
    parser.add_argument(
        '--output', required=True, type=Path,
        help='Output file: one absolute test_*.cpp path per line, read by CMake via TEST_LIST_FILE')
    parser.add_argument(
        '--num_shards', dest='num_shards', required=False, default=1, type=int,
        help='Total number of shards (default: 1 = all tests)')
    parser.add_argument(
        '--shard', dest='shard', required=False, default=0, type=int,
        help='Zero-based shard index (default: 0)')
    parser.add_argument(
        '--filters', nargs='+', required=False, default=[], type=str,
        help='Only include test_*.cpp files whose directory (relative to --test-dirpath) matches one '
             'of these filters. Prefix with @ to read filters from a file (one per line). Directory '
             'matching mirrors scripts/regression.py: a plain alphanumeric filter must match the '
             'directory exactly; anything else is treated as a case-insensitive regex searched '
             'against the directory. Empty/absent filters (the default) include everything.')
    args = parser.parse_args()
    args.filters = build_filters(args.filters)

    if args.num_shards < 1:
        print('error: --num_shards must be >= 1', file=sys.stderr)
        return 1
    if not (0 <= args.shard < args.num_shards):
        print(f'error: --shard {args.shard} out of range 0..{args.num_shards - 1}', file=sys.stderr)
        return 1

    if not args.test_dirpath.is_absolute():
        args.test_dirpath = (_workspace_dirpath / args.test_dirpath).resolve()
    else:
        args.test_dirpath = args.test_dirpath.resolve()

    if not args.test_dirpath.is_dir():
        print(f'error: test directory not found: {args.test_dirpath}', file=sys.stderr)
        return 1

    selected, total = _scan(args.test_dirpath, args.filters, args.shard, args.num_shards)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w') as strm:
        for src in selected:
            strm.write(src.as_posix() + '\n')

    print(f' command-line: {" ".join(sys.argv)}')
    print(f' test-dirpath: {args.test_dirpath}')
    print(f'      filters: {len(args.filters)} ({"none, all tests included" if not args.filters else "narrowed"})')
    print(f'       output: {args.output}')
    print(f'        shard: {args.shard}/{args.num_shards} ({len(selected)}/{total} tests selected)')
    print(f'      folders: {len(_find_owners(args.test_dirpath))} with their own CMakeLists.txt (each kept in one shard)')
    return 0


if __name__ == '__main__':
    sys.exit(_main())
