#!/usr/bin/env python3

import argparse
import sys
from pathlib import Path

_this_filepath = Path(__file__).resolve()
_workspace_dirpath = _this_filepath.parent.parent
_default_test_dirpath = _workspace_dirpath / 'hlc'


def _dumb_hash(name: str) -> int:
    return sum(ord(c) for c in name)


def _scan(test_dirpath: Path, shard: int, num_shards: int) -> tuple[list[Path], int]:
    all_sources = sorted(test_dirpath.rglob('test_*.cpp'), key=lambda p: p.as_posix().lower())
    selected = [
        src for src in all_sources
        if (_dumb_hash(src.parent.relative_to(test_dirpath).as_posix()) % num_shards) == shard
    ]
    return selected, len(all_sources)


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
    args = parser.parse_args()

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

    selected, total = _scan(args.test_dirpath, args.shard, args.num_shards)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w') as strm:
        for src in selected:
            strm.write(src.as_posix() + '\n')

    print(f' command-line: {" ".join(sys.argv)}')
    print(f' test-dirpath: {args.test_dirpath}')
    print(f'       output: {args.output}')
    print(f'        shard: {args.shard}/{args.num_shards} ({len(selected)}/{total} tests selected)')
    return 0


if __name__ == '__main__':
    sys.exit(_main())
