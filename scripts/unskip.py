#!/usr/bin/env python3
"""Find GTEST_SKIP()s that no longer need to be there.

Used by .github/workflows/unskip.yml in three steps:

  disable  Comment out every handled GTEST_SKIP() statement in hlc/**/test_*.cpp and write a
           shard_tests.py filter list naming only the test directories that had one, so only those
           tests are built and run.
  collect  Read the gtest XML reports (GTEST_OUTPUT=xml:<dir>/) of that run and record the result
           of every test case that had a handled skip.
  apply    On a clean checkout, delete the GTEST_SKIP() statements of the test cases that passed,
           and write a markdown summary for the pull request.

A skip is "handled" only when it is an unconditional statement directly in a TEST/TEST_F/TEST_P
body, starting its own line and ending its line (a trailing // comment is allowed). Anything else
-- a skip under an if/else, inside a nested block, in a fixture's SetUp or a helper -- is left
untouched in every step: removing it automatically could change what the test does. A skip is
also left alone when its test has no EXPECT_*/ASSERT_* outside the skip: such a placeholder passes
trivially once the skip is gone.

Test cases are identified as "<executable>::<suite>::<name>"; the executable is the test source's
file stem, as in CMakeLists.txt.
"""

import argparse
import json
import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

_this_filepath = Path(__file__).resolve()
_workspace_dirpath = _this_filepath.parent.parent
_default_test_dirpath = _workspace_dirpath / 'hlc'

_TEST_MACROS = {'TEST', 'TEST_F', 'TEST_P', 'TYPED_TEST', 'TYPED_TEST_P'}
_IDENT_RE = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
_NUMBER_RE = re.compile(r"[0-9][0-9A-Za-z_.']*")
_RAW_STRING_RE = re.compile(r'(?:u8|u|U|L)?R"([^()\\\s]{0,16})\(')
_STRING_PREFIX_RE = re.compile(r'(?:u8|u|U|L)(?=["\'])')
_DISABLED_MARKER = '// [unskip] '


@dataclass
class Token:
    kind: str  # 'ident', 'punct', 'literal'
    value: str
    start: int
    end: int


@dataclass
class Skip:
    filepath: Path
    suite: str
    name: str
    start_line: int  # 0-based, inclusive
    end_line: int  # 0-based, inclusive
    handled: bool
    reason: str = ''

    @property
    def key(self) -> str:
        return f'{self.filepath.stem}::{self.suite}::{self.name}'


@dataclass
class FileSkips:
    filepath: Path
    skips: list[Skip] = field(default_factory=list)


def _tokenize(text: str) -> list[Token]:
    """Tokenize C++ source enough to find code outside comments and literals."""
    tokens: list[Token] = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c.isspace():
            i += 1
            continue
        if text.startswith('//', i):
            j = text.find('\n', i)
            i = n if j < 0 else j
            continue
        if text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
            continue
        m = _RAW_STRING_RE.match(text, i)
        if m:
            terminator = ')' + m.group(1) + '"'
            j = text.find(terminator, m.end())
            j = n if j < 0 else j + len(terminator)
            tokens.append(Token('literal', text[i:j], i, j))
            i = j
            continue
        m = _STRING_PREFIX_RE.match(text, i)
        start = i
        if m:
            i = m.end()
            c = text[i]
        if c in '"\'':
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                if text[j] == '\\':
                    j += 1
                j += 1
            j = min(j + 1, n)
            tokens.append(Token('literal', text[start:j], start, j))
            i = j
            continue
        if c.isalpha() or c == '_':
            m = _IDENT_RE.match(text, i)
            tokens.append(Token('ident', m.group(0), i, m.end()))
            i = m.end()
            continue
        if c.isdigit():
            m = _NUMBER_RE.match(text, i)
            tokens.append(Token('literal', m.group(0), i, m.end()))
            i = m.end()
            continue
        tokens.append(Token('punct', c, i, i + 1))
        i += 1
    return tokens


def _match_close(tokens: list[Token], index: int) -> int:
    """Index of the token closing the bracket at tokens[index], or -1."""
    pairs = {'(': ')', '[': ']', '{': '}'}
    stack = []
    for k in range(index, len(tokens)):
        value = tokens[k].value
        if tokens[k].kind != 'punct':
            continue
        if value in pairs:
            stack.append(pairs[value])
        elif value in (')', ']', '}'):
            if not stack or stack[-1] != value:
                return -1
            stack.pop()
            if not stack:
                return k
    return -1


def _line_of(text: str, offset: int) -> int:
    return text.count('\n', 0, offset)


def _owns_lines(text: str, start: int, end: int) -> bool:
    """True when [start, end) starts its line and nothing but a // comment follows it."""
    line_begin = text.rfind('\n', 0, start) + 1
    if text[line_begin:start].strip():
        return False
    line_end = text.find('\n', end)
    rest = text[end:len(text) if line_end < 0 else line_end].strip()
    return not rest or rest.startswith('//')


def _scan_body(filepath: Path, text: str, tokens: list[Token], open_index: int, close_index: int,
               suite: str, name: str) -> list[Skip]:
    skips: list[Skip] = []
    skip_ranges: list[tuple[int, int]] = []
    depth = 0
    k = open_index
    while k < close_index:
        tok = tokens[k]
        if tok.kind == 'punct' and tok.value == '{':
            depth += 1
        elif tok.kind == 'punct' and tok.value == '}':
            depth -= 1
        elif tok.kind == 'ident' and tok.value == 'GTEST_SKIP':
            # The statement ends at the first ';' outside any bracket.
            end_index = k
            nesting = 0
            while end_index < close_index:
                value = tokens[end_index].value if tokens[end_index].kind == 'punct' else ''
                if value in ('(', '[', '{'):
                    nesting += 1
                elif value in (')', ']', '}'):
                    nesting -= 1
                elif value == ';' and nesting == 0:
                    break
                end_index += 1
            start = tok.start
            end = tokens[end_index].end
            prev = tokens[k - 1]
            reason = ''
            if depth != 1:
                reason = 'inside a nested block'
            elif not (prev.kind == 'punct' and prev.value in ('{', ';', '}')):
                reason = f'not an unconditional statement (follows "{prev.value}")'
            elif end_index >= close_index:
                reason = 'statement end not found'
            elif not _owns_lines(text, start, end):
                reason = 'shares its line with other code'
            skips.append(Skip(filepath, suite, name, _line_of(text, start), _line_of(text, end),
                              not reason, reason))
            skip_ranges.append((k, end_index))
            k = end_index
        k += 1

    # A body that checks nothing besides its skip (a placeholder) passes trivially once the skip
    # is gone; that pass says nothing, so such a skip is never removed.
    def _in_skip(index: int) -> bool:
        return any(lo <= index <= hi for lo, hi in skip_ranges)

    checks = any(tokens[i].kind == 'ident' and tokens[i].value.startswith(('EXPECT_', 'ASSERT_'))
                 and not _in_skip(i) for i in range(open_index, close_index))
    if not checks:
        for skip in skips:
            if skip.handled:
                skip.handled = False
                skip.reason = 'the test checks nothing besides the skip'
    return skips


def _opens_namespace(tokens: list[Token], index: int) -> bool:
    """True when the '{' at tokens[index] opens a namespace (named, nested or anonymous)."""
    k = index - 1
    while k >= 0 and (tokens[k].kind == 'ident' and tokens[k].value != 'namespace'
                      or tokens[k].value == ':'):
        k -= 1
    return k >= 0 and tokens[k].value == 'namespace'


def scan_file(filepath: Path) -> FileSkips:
    # Read through "disable"'s marker, so "collect" still sees the skips it commented out. The
    # marker is only ever inserted after a line's indentation, so removing it restores the line.
    text = filepath.read_text(encoding='utf-8').replace(_DISABLED_MARKER, '')
    result = FileSkips(filepath)
    if 'GTEST_SKIP' not in text:
        return result
    tokens = _tokenize(text)
    # Only braces that are not namespaces count: tests live inside "namespace hlc { ... }".
    braces: list[bool] = []  # True for a namespace brace
    k = 0
    while k < len(tokens):
        tok = tokens[k]
        if tok.kind == 'punct' and tok.value == '{':
            braces.append(_opens_namespace(tokens, k))
        elif tok.kind == 'punct' and tok.value == '}':
            if braces:
                braces.pop()
        elif (all(braces) and tok.kind == 'ident' and tok.value in _TEST_MACROS
              and k + 1 < len(tokens) and tokens[k + 1].value == '('):
            args_close = _match_close(tokens, k + 1)
            if args_close < 0 or args_close + 1 >= len(tokens) or tokens[args_close + 1].value != '{':
                k += 1
                continue
            args = [t.value for t in tokens[k + 2:args_close] if t.kind == 'ident']
            body_close = _match_close(tokens, args_close + 1)
            if len(args) < 2 or body_close < 0:
                k += 1
                continue
            result.skips.extend(
                _scan_body(filepath, text, tokens, args_close + 1, body_close, args[0], args[-1]))
            k = body_close
        elif tok.kind == 'ident' and tok.value == 'GTEST_SKIP':
            line = _line_of(text, tok.start)
            result.skips.append(Skip(filepath, '', '', line, line, False, 'outside a test body'))
        k += 1
    return result


def scan(test_dirpath: Path) -> list[FileSkips]:
    sources = sorted(test_dirpath.rglob('test_*.cpp'), key=lambda p: p.as_posix().lower())
    return [fs for fs in (scan_file(src) for src in sources) if fs.skips]


def _read_lines(filepath: Path) -> list[str]:
    with filepath.open('r', encoding='utf-8', newline='') as strm:
        return strm.readlines()


def _write_lines(filepath: Path, lines: list[str]) -> None:
    with filepath.open('w', encoding='utf-8', newline='') as strm:
        strm.writelines(lines)


def _rel(filepath: Path) -> str:
    # Relative to the current directory, not to this script: the workflow runs a copy of the
    # script from another checkout.
    try:
        return filepath.relative_to(Path.cwd().resolve()).as_posix()
    except ValueError:
        return filepath.as_posix()


def _cmd_disable(args) -> int:
    file_skips = scan(args.test_dirpath)
    handled = 0
    unhandled = 0
    dirs: set[str] = set()
    for fs in file_skips:
        lines = _read_lines(fs.filepath)
        changed = False
        for skip in fs.skips:
            if not skip.handled:
                unhandled += 1
                print(f'  left alone: {_rel(fs.filepath)}:{skip.start_line + 1}: {skip.reason}')
                continue
            handled += 1
            changed = True
            for index in range(skip.start_line, skip.end_line + 1):
                line = lines[index]
                indent = len(line) - len(line.lstrip(' \t'))
                lines[index] = line[:indent] + _DISABLED_MARKER + line[indent:]
        if changed:
            _write_lines(fs.filepath, lines)
            dirs.add(fs.filepath.parent.relative_to(args.test_dirpath).as_posix())

    # Anchored and escaped: shard_tests.py treats a non-alphanumeric filter as a regex searched in
    # the directory path, so a bare path would also match longer sibling directories.
    args.filter_list.parent.mkdir(parents=True, exist_ok=True)
    with args.filter_list.open('w', encoding='utf-8') as strm:
        for d in sorted(dirs):
            strm.write(f'^{re.escape(d)}$\n')

    print(f'Disabled {handled} GTEST_SKIP() statement(s) in {len(dirs)} test director(y/ies); '
          f'left {unhandled} alone.')
    print(f'Filter list: {args.filter_list}')
    return 0


def _testcase_status(testcase: ET.Element) -> str:
    if testcase.find('failure') is not None or testcase.find('error') is not None:
        return 'failed'
    if testcase.find('skipped') is not None or testcase.get('result') == 'skipped':
        return 'skipped'
    if testcase.get('status', 'run') != 'run':
        return 'not run'
    return 'passed'


def _cmd_collect(args) -> int:
    file_skips = scan(args.test_dirpath)
    if args.test_list:
        # Only the test files this shard built: the others belong to another shard's report.
        listed = {Path(line.strip()).resolve()
                  for line in args.test_list.read_text(encoding='utf-8').splitlines() if line.strip()}
        file_skips = [fs for fs in file_skips if fs.filepath.resolve() in listed]
    wanted = {s.key for fs in file_skips for s in fs.skips if s.handled}
    results: dict[str, str] = {}
    for xml_filepath in sorted(args.xml_dirpath.glob('*.xml')):
        try:
            root = ET.parse(xml_filepath).getroot()
        except ET.ParseError as e:
            print(f'warning: cannot parse {xml_filepath}: {e}', file=sys.stderr)
            continue
        stem = xml_filepath.stem
        for testcase in root.iter('testcase'):
            key = f'{stem}::{testcase.get("classname")}::{testcase.get("name")}'
            if key in wanted:
                results[key] = _testcase_status(testcase)
    # A test whose executable did not build, crashed or timed out writes no report.
    for key in wanted:
        results.setdefault(key, 'no result')

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w', encoding='utf-8') as strm:
        json.dump(dict(sorted(results.items())), strm, indent=2)

    counts: dict[str, int] = {}
    for status in results.values():
        counts[status] = counts.get(status, 0) + 1
    print(f'{len(results)} test case(s) with a disabled skip: '
          + ', '.join(f'{v} {k}' for k, v in sorted(counts.items())))
    return 0


def _stale_comment_end(lines: list[str], index: int) -> int:
    """End (exclusive) of a // comment block at lines[index] that talks about the skip above."""
    if index >= len(lines) or not lines[index].lstrip().startswith('//') \
            or 'GTEST_SKIP' not in lines[index]:
        return index
    while index < len(lines) and lines[index].lstrip().startswith('//'):
        index += 1
    return index


def _cmd_apply(args) -> int:
    # When reports overlap, the worst real result wins; "no result" (not built or run there) never
    # hides a real one, and "passed" counts only if no report has anything worse.
    rank = {'no result': 0, 'passed': 1, 'not run': 2, 'skipped': 3, 'failed': 4}
    results: dict[str, str] = {}
    for results_filepath in args.results:
        with results_filepath.open(encoding='utf-8') as strm:
            for key, status in json.load(strm).items():
                if rank.get(status, 4) >= rank.get(results.get(key, 'no result'), 4):
                    results[key] = status

    removed: list[Skip] = []
    still_skipped: list[tuple[Skip, str]] = []
    left_alone = 0
    for fs in scan(args.test_dirpath):
        lines = _read_lines(fs.filepath)
        drop: set[int] = set()
        for skip in fs.skips:
            if not skip.handled:
                left_alone += 1
                continue
            status = results.get(skip.key, 'no result')
            if status != 'passed':
                still_skipped.append((skip, status))
                continue
            removed.append(skip)
            drop.update(range(skip.start_line, skip.end_line + 1))
            drop.update(range(skip.end_line + 1, _stale_comment_end(lines, skip.end_line + 1)))
        if drop:
            _write_lines(fs.filepath, [line for i, line in enumerate(lines) if i not in drop])

    print(f'Removed {len(removed)} GTEST_SKIP() statement(s); {len(still_skipped)} still needed.')

    if args.summary:
        counts: dict[str, int] = {}
        for _, status in still_skipped:
            counts[status] = counts.get(status, 0) + 1
        tested = len(removed) + len(still_skipped)
        # Skipped anyway (e.g. by a conditional skip left alone in the same test) or not run.
        other = counts.get('skipped', 0) + counts.get('not run', 0)
        rows = [
            ('GTEST_SKIPs found', tested + left_alone),
            ('Left alone (not tested)', left_alone),
            ('Tested', tested),
            ('Passed &rarr; skip removed', len(removed)),
            ('Failed &rarr; skip kept', counts.get('failed', 0)),
        ]
        if other:
            rows.append(('Still skipped (another skip in the test)', other))
        rows.append(('No result (build/crash/timeout)', counts.get('no result', 0)))

        out = ['## Unskip summary', '', '| Result | Count |', '|---|---:|']
        out.extend(f'| {label} | {value} |' for label, value in rows)
        out.append('')
        out.append('## Unskipped tests')
        out.append('')
        out.append(f'These test cases pass with their `GTEST_SKIP()` removed, so the skip is gone '
                   f'({len(removed)} skip(s) in '
                   f'{len({s.filepath for s in removed})} file(s)).')
        out.append('')
        out.append('Review each one: a test that passes without its skip may still assert the '
                   'wrong thing, and comments that mention the old skip may need updating.')
        out.append('')
        current = None
        for skip in sorted(removed, key=lambda s: (_rel(s.filepath), s.start_line)):
            if skip.filepath != current:
                current = skip.filepath
                out.append(f'- `{_rel(skip.filepath)}`')
            out.append(f'  - `{skip.suite}.{skip.name}`')
        if not removed:
            out.append('None.')
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text('\n'.join(out) + '\n', encoding='utf-8')

    if args.count_file:
        args.count_file.write_text(f'{len(removed)}\n', encoding='utf-8')
    return 0


def _main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--test-dirpath', dest='test_dirpath', default=_default_test_dirpath,
                        type=Path, help='Root directory of test_*.cpp files (default: hlc/)')
    sub = parser.add_subparsers(dest='command', required=True)

    p = sub.add_parser('disable', help='Comment out handled GTEST_SKIP()s, write a filter list')
    p.add_argument('--filter-list', dest='filter_list', required=True, type=Path,
                   help='Output: shard_tests.py --filters file of the affected test directories')

    p = sub.add_parser('collect', help='Record results of test cases with a disabled skip')
    p.add_argument('--xml-dirpath', dest='xml_dirpath', required=True, type=Path,
                   help='Directory of gtest XML reports')
    p.add_argument('--output', required=True, type=Path, help='Output JSON: test case -> status')
    p.add_argument('--test-list', dest='test_list', type=Path,
                   help='shard_tests.py output: report only the test files listed in it')

    p = sub.add_parser('apply', help='Delete the GTEST_SKIP()s of test cases that passed')
    p.add_argument('--results', nargs='+', required=True, type=Path,
                   help='JSON files written by "collect"')
    p.add_argument('--summary', type=Path, help='Output: markdown summary for the pull request')
    p.add_argument('--count-file', dest='count_file', type=Path,
                   help='Output: number of removed skips')

    args = parser.parse_args()
    if not args.test_dirpath.is_absolute():
        args.test_dirpath = (_workspace_dirpath / args.test_dirpath)
    args.test_dirpath = args.test_dirpath.resolve()
    if not args.test_dirpath.is_dir():
        print(f'error: test directory not found: {args.test_dirpath}', file=sys.stderr)
        return 1

    return {'disable': _cmd_disable, 'collect': _cmd_collect, 'apply': _cmd_apply}[args.command](args)


if __name__ == '__main__':
    sys.exit(_main())
