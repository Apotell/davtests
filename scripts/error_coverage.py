#!/usr/bin/env python3

"""
Error coverage report for davtests.

Joins three sources:
  1. The HLC error code table (ErrorDefinition.h/.cpp in the hlc repo).
  2. The diagnostics each test reports (golden <name>.log, or a fresh regression output).
  3. The diagnostics each test_*.cpp asserts via Test::findError().

and writes report.xlsx (detail) and summary.md (CI job summary).

Classification of a reported diagnostic (test x code x location):
  Checked        - a non-skipped expect-present assertion matches it
  Satisfied (skipped) - only a skipped expect-present assertion matches it
  Contradiction  - a non-skipped expect-absent assertion matches it
  Known bogus    - only a skipped expect-absent assertion matches it (tracked HLC defect)
  Not checked    - no assertion matches it (incl. tests without a cpp)

Classification of an assertion:
  Checked        - expect-present, matched a report
  Pending        - expect-present, skipped, no report (known missing diagnostic), or
                   expect-absent, skipped, reported (known bogus diagnostic)
  Broken         - expect-present, not skipped, no report (the test should be failing)
  Alternative    - expect-present member of an any-of group that another member satisfied
  Guard          - expect-absent, no report
  Conditional    - inside an if/else and does not hold against this log (not counted as broken/contradiction)
  Contradiction  - expect-absent, not skipped, matched a report
  Satisfied (skipped) - skipped, but already holds: the diagnostic is reported (present) or
                   no longer reported (absent)

A skipped TEST whose findError checks all hold is listed as a stale-skip candidate.

Per error code (exactly one bucket, in priority order):
  Checked, Not checked (reported but no test checks it), Pending, Guard-only, Dead
"""

import argparse
import multiprocessing
import os
import re
import shlex
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

_this_dirpath = Path(__file__).parent.resolve()
_workspace_dirpath = _this_dirpath.parent

_main_severities = ('FTL', 'SNT', 'ERR', 'WRN')
_note_severities = ('NTE',)
_counted_severities = _main_severities + _note_severities

# ErrorDefinition::getCategoryName(): the two-letter prefix printed in the log, e.g. [WRN:CP5851].
_category_prefixes = {
  'DB': 'DB', 'CMD': 'CM', 'PP': 'PP', 'PARSE': 'PA', 'PYTH': 'PY', 'COMP': 'CP', 'ELAB': 'EL',
  'LIB': 'LB', 'LINT': 'LN', 'USER': 'US'
}

_severity_tags = {
  'FATAL': 'FTL', 'SYNTAX': 'SNT', 'ERROR': 'ERR', 'WARNING': 'WRN', 'INFO': 'INF', 'NOTE': 'NTE'
}

# Paths (relative to a -wd root, or resolved) that belong to a third-party library
# rather than to the test itself.
_default_external_pattern = (
  r'(^|/)(third_party/|(ovm|uvm|uvmc)-[0-9][^/]*/|1800\.2-[0-9][^/]*/)'
)

_re_diag = re.compile(r'^\[(?P<sev>[A-Z]{3}):(?P<prefix>[A-Z]{2})(?P<num>\d+)\] (?P<rest>.*)$')
_re_location = re.compile(r'^(?P<path>[$/\\A-Za-z0-9_.][^ ]*?)(?::(?P<line>\d+):(?:(?P<col>\d+):)?)? (?P<msg>.*)$')
_re_continuation = re.compile(r'^\s+(?P<path>\S+?):(?P<line>\d+):(?:(?P<col>\d+):)?\s')
_re_quoted = re.compile(r'"([^"]*)"')


# --------------------------------------------------------------------------------------------------
# Code table
# --------------------------------------------------------------------------------------------------

@dataclass
class Code:
  name: str
  number: int
  severity: str = ''      # FTL/SNT/ERR/WRN/INF/NTE
  category: str = ''      # DB/CMD/PP/...
  text: str = ''
  symbol_re: re.Pattern | None = None


def _load_code_table(hlc_repo: Path) -> dict[str, Code]:
  # The repo tracks include/hlc (lowercase); an install tree may use include/Hlc.
  header = next((hlc_repo / 'include' / d / 'ErrorReporting' / 'ErrorDefinition.h' for d in ('hlc', 'Hlc')
                 if (hlc_repo / 'include' / d / 'ErrorReporting' / 'ErrorDefinition.h').is_file()),
                hlc_repo / 'include' / 'hlc' / 'ErrorReporting' / 'ErrorDefinition.h')
  source = hlc_repo / 'src' / 'ErrorReporting' / 'ErrorDefinition.cpp'
  if not header.is_file() or not source.is_file():
    raise FileNotFoundError(f'ErrorDefinition.h/.cpp not found under {hlc_repo}')

  text = header.read_text(encoding='utf-8')
  m = re.search(r'enum\s+ErrorType\s*\{(.*?)\};', text, re.S)
  if not m:
    raise RuntimeError(f'enum ErrorType not found in {header}')

  codes = {}
  for name, number in re.findall(r'\b([A-Z][A-Z0-9_]*)\s*=\s*(\d+)', m.group(1)):
    if name != 'NO_ERROR_MESSAGE':
      codes[name] = Code(name, int(number))

  entry = re.compile(
    r'forward_as_tuple\(\s*([A-Z][A-Z0-9_]*)\s*\)\s*,\s*std::forward_as_tuple\(\s*([A-Z]+)\s*,\s*([A-Z]+)\s*,\s*'
    r'"((?:[^"\\]|\\.)*)"'
  )
  for name, severity, category, msg in entry.findall(source.read_text(encoding='utf-8')):
    code = codes.get(name)
    if code is None:
      continue
    code.severity = _severity_tags.get(severity, severity)
    code.category = category
    code.text = msg.encode('utf-8').decode('unicode_escape')
    code.symbol_re = _make_symbol_regex(code.text)

  return codes


def _make_symbol_regex(text: str):
  # The renderer substitutes the primary location's object into the first %s only.
  index = text.find('%s')
  if index < 0:
    return None
  pattern = re.escape(text[:index]) + '(?P<sym>.*)' + re.escape(text[index + 2:])
  return re.compile('^' + pattern + '$', re.S)


# --------------------------------------------------------------------------------------------------
# Tests (.hlc) and their scope roots
# --------------------------------------------------------------------------------------------------

@dataclass
class TestCase:
  test_id: str            # directory of the .hlc relative to the test dir (as regression.py uses)
  name: str               # .hlc stem
  hlc_filepath: Path
  wd_dirpaths: list = field(default_factory=list)
  cpp_filepaths: list = field(default_factory=list)


def _parse_hlc(filepath: Path) -> list[Path]:
  try:
    tokens = shlex.split(filepath.read_text(encoding='utf-8', errors='replace'), posix=True, comments=True)
  except ValueError:
    tokens = filepath.read_text(encoding='utf-8', errors='replace').split()
  wds = []
  for i, token in enumerate(tokens):
    if token == '-wd' and i + 1 < len(tokens):
      wds.append((filepath.parent / tokens[i + 1]).resolve())
  return wds


def _scan_tests(test_dirpath: Path, filter_re) -> dict[str, TestCase]:
  tests = {}
  for filepath in sorted(test_dirpath.rglob('*.hlc')):
    test_id = filepath.parent.relative_to(test_dirpath).as_posix()
    if filter_re and not filter_re.search(test_id):
      continue
    tests[test_id] = TestCase(test_id, filepath.stem, filepath, _parse_hlc(filepath))
  return tests


# --------------------------------------------------------------------------------------------------
# Reported diagnostics (log files)
# --------------------------------------------------------------------------------------------------

@dataclass
class Report:
  test_id: str
  severity: str
  prefix: str
  number: int
  code: str               # enum name, or '?<prefix><number>' when unknown
  path: str
  line: int
  col: int
  message: str
  symbol: str | None
  scope: str              # local / external
  extra_locations: list = field(default_factory=list)   # [(line, col)]
  status: str = ''
  matched_by: list = field(default_factory=list)        # assertion ids


def _find_log(log_dirpath: Path, test: TestCase):
  dirpath = log_dirpath / test.test_id
  for suffix in ('', '.linux', '.osx', '.msys', '.win'):
    filepath = dirpath / f'{test.name}{suffix}.log'
    if filepath.is_file():
      return filepath
  # The regression harness names the log after the test directory, not the .hlc stem.
  for suffix in ('', '.linux', '.osx', '.msys', '.win'):
    filepath = dirpath / f'{dirpath.name}{suffix}.log'
    if filepath.is_file():
      return filepath
  return None


_platform_ids = ('.linux', '.osx', '.msys', '.win', '')


def _extract_regression_archive(args):
  """Copies the diagnostic lines of every test log in one regression shard artifact
  (Regression_*.zip, as uploaded by the regression job) to <out>/<test_id>/<name>.log.
  Only diagnostics and their continuation lines are kept, so the HLDB dumps never hit the disk."""
  import io
  import tarfile
  import zipfile

  zip_filepath, out_dirpath = args
  archive_name = Path(zip_filepath).stem
  count = 0
  with zipfile.ZipFile(zip_filepath, 'r') as zip_strm:
    with zip_strm.open(f'{archive_name}.tar.gz') as tar_strm:
      with tarfile.open(fileobj=tar_strm, mode='r|gz') as archive_strm:
        for member in archive_strm:
          if not member.isfile() or not member.name.endswith('.tar.gz'):
            continue
          # <archive>/<test_id>.tar.gz holds <name>/<name>.log, name being the last part of test_id.
          test_id = Path(member.name[:-len('.tar.gz')]).relative_to(archive_name).as_posix()
          name = Path(test_id).name
          data = archive_strm.extractfile(member).read()
          with tarfile.open(fileobj=io.BytesIO(data)) as test_strm:
            names = set(test_strm.getnames())
            for platform_id in _platform_ids:
              log_name = f'{name}/{name}{platform_id}.log'
              if log_name in names:
                break
            else:
              continue
            kept = []
            in_diag = False
            for raw in test_strm.extractfile(log_name):
              line = raw.decode('utf-8', errors='replace')
              if line.startswith('['):
                in_diag = _re_diag.match(line.rstrip('\r\n')) is not None
                if in_diag:
                  kept.append(line)
              elif in_diag and line[:1] in (' ', '\t'):
                kept.append(line)
              else:
                in_diag = False
          dirpath = Path(out_dirpath) / test_id
          dirpath.mkdir(parents=True, exist_ok=True)
          (dirpath / f'{name}.log').write_text(''.join(kept), encoding='utf-8')
          count += 1
  return zip_filepath, count


class _ScopeResolver:
  def __init__(self, wd_dirpaths, external_re, workspace_dirpath: Path):
    self.m_wds = wd_dirpaths
    self.m_external_re = external_re
    self.m_workspace = workspace_dirpath
    self.m_cache = {}

  def scope(self, path: str) -> str:
    cached = self.m_cache.get(path)
    if cached is not None:
      return cached

    result = 'local'
    norm = path.replace('\\', '/')
    m = re.match(r'^\$Source\d+/(.*)$', norm)
    if m:
      rel = m.group(1)
      resolved = None
      for wd in self.m_wds:
        candidate = wd / rel
        if candidate.exists():
          resolved = candidate
          break
      probe = rel
      if resolved is not None:
        try:
          probe = resolved.relative_to(self.m_workspace).as_posix()
        except ValueError:
          probe = resolved.as_posix()
      if self.m_external_re.search(probe) or self.m_external_re.search(rel):
        result = 'external'

    self.m_cache[path] = result
    return result


def _parse_log(args):
  test, log_filepath, code_by_number, external_pattern, workspace_dirpath = args
  resolver = _ScopeResolver(test.wd_dirpaths, re.compile(external_pattern, re.I), workspace_dirpath)
  reports = []
  last = None
  with open(log_filepath, encoding='utf-8', errors='replace') as strm:
    for raw in strm:
      if raw.startswith('['):
        m = _re_diag.match(raw.rstrip('\r\n'))
        if not m or m.group('sev') not in _counted_severities:
          last = None
          continue

        number = int(m.group('num'))
        code = code_by_number.get(number)
        rest = m.group('rest')
        path, line, col, message = '', 0, 0, rest
        lm = _re_location.match(rest)
        if lm and (lm.group('path').startswith(('$', '/', '\\')) or '/' in lm.group('path')):
          path = lm.group('path')
          line = int(lm.group('line') or 0)
          col = int(lm.group('col') or 0)
          message = lm.group('msg')

        stripped = message[:-1] if message.endswith(('.', ',')) else message
        symbol = None
        if code is not None and code.symbol_re is not None:
          sm = code.symbol_re.match(stripped)
          if sm:
            symbol = sm.group('sym')
        if symbol is None:
          qm = _re_quoted.search(message)
          if qm:
            symbol = qm.group(1)

        last = Report(
          test_id=test.test_id,
          severity=m.group('sev'),
          prefix=m.group('prefix'),
          number=number,
          code=code.name if code else f'?{m.group("prefix")}{m.group("num")}',
          path=path, line=line, col=col,
          message=message,
          symbol=symbol,
          scope=resolver.scope(path) if path else 'local',
        )
        reports.append(last)
      elif last is not None and raw[:1] in (' ', '\t'):
        cm = _re_continuation.match(raw)
        if cm:
          last.extra_locations.append((int(cm.group('line')), int(cm.group('col') or 0)))
        else:
          last = None
      else:
        last = None
  return test.test_id, reports


# --------------------------------------------------------------------------------------------------
# Asserted diagnostics (test_*.cpp static scan)
# --------------------------------------------------------------------------------------------------

@dataclass
class Assertion:
  aid: int
  test_id: str
  cpp_filepath: str
  cpp_line: int
  test_name: str          # Fixture.Name, or helper name for unresolved helpers
  code: str
  symbol: str | None      # None = not narrowed by symbol
  line: int
  col: int
  polarity: str           # present / absent
  skipped: bool
  source: str             # direct / variable / helper
  group: int              # any-of group id (0 = none)
  conditional: bool = False   # inside an if/else, so it may not run in every configuration
  status: str = ''
  matched: list = field(default_factory=list)


@dataclass
class Unparsed:
  test_id: str
  cpp_filepath: str
  cpp_line: int
  context: str
  reason: str


def _mask_cpp(text: str):
  """Returns (code, masked): comments blanked in both; string/char contents also blanked in masked."""
  code = list(text)
  masked = list(text)
  i, n = 0, len(text)
  while i < n:
    c = text[i]
    if c == '/' and i + 1 < n and text[i + 1] == '/':
      j = text.find('\n', i)
      j = n if j < 0 else j
      for k in range(i, j):
        code[k] = masked[k] = ' '
      i = j
    elif c == '/' and i + 1 < n and text[i + 1] == '*':
      j = text.find('*/', i + 2)
      j = n if j < 0 else j + 2
      for k in range(i, j):
        if text[k] != '\n':
          code[k] = masked[k] = ' '
      i = j
    elif c == 'R' and text.startswith('R"', i):
      m = re.match(r'R"([^(\s]*)\(', text[i:])
      if m:
        end = text.find(')' + m.group(1) + '"', i)
        end = n if end < 0 else end + len(m.group(1)) + 2
        for k in range(i + 2, end - 1):
          if text[k] != '\n':
            masked[k] = ' '
        i = end
      else:
        i += 1
    elif c in '"\'':
      j = i + 1
      while j < n and text[j] != c and text[j] != '\n':
        j += 2 if text[j] == '\\' else 1
      for k in range(i + 1, min(j, n)):
        masked[k] = ' '
      i = j + 1
    else:
      i += 1
  return ''.join(code), ''.join(masked)


def _match_close(masked: str, start: int, open_ch: str, close_ch: str) -> int:
  depth = 0
  for i in range(start, len(masked)):
    if masked[i] == open_ch:
      depth += 1
    elif masked[i] == close_ch:
      depth -= 1
      if depth == 0:
        return i
  return len(masked) - 1


def _split_args(code: str, masked: str, start: int, end: int):
  args, depth, last = [], 0, start
  for i in range(start, end):
    ch = masked[i]
    if ch in '([{':
      depth += 1
    elif ch in ')]}':
      depth -= 1
    elif ch == ',' and depth == 0:
      args.append(code[last:i].strip())
      last = i + 1
  tail = code[last:end].strip()
  if tail or args:
    args.append(tail)
  return args


_control_keywords = {'if', 'for', 'while', 'switch', 'catch', 'do', 'else', 'return', 'sizeof'}


@dataclass
class _Region:
  kind: str               # test / function / class / block
  name: str
  start: int              # offset of '{'
  end: int                # offset of matching '}'
  fixture: str = ''


def _find_regions(masked: str) -> list[_Region]:
  regions = []
  stack = []
  header_start = 0
  for i, ch in enumerate(masked):
    if ch == '{':
      header = masked[header_start:i].strip()
      region = _Region('block', '', i, -1)
      m = re.search(r'\bTEST(?:_F|_P)?\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*$', header)
      if m:
        region.kind, region.name, region.fixture = 'test', f'{m.group(1)}.{m.group(2)}', m.group(1)
      else:
        mc = re.search(r'\b(?:class|struct)\s+(\w+)[^;()]*$', header)
        if mc:
          region.kind, region.name = 'class', mc.group(1)
        else:
          mf = re.search(r'(?:^|[\s*&:~])(\w+)\s*\((?:[^()]|\([^()]*\))*\)\s*(?:const|override|noexcept|final|\s)*(?::[^{}]*)?$', header)
          if mf and mf.group(1) not in _control_keywords and not re.search(r'[=\]]\s*\(', header[-200:]):
            region.kind, region.name = 'function', mf.group(1)
          elif re.search(r'(^|[\s;}])(if\s*\(.*\)|else)\s*$', header, re.S):
            region.kind = 'cond'
      stack.append(region)
      regions.append(region)
      header_start = i + 1
    elif ch == '}':
      if stack:
        stack.pop().end = i
      header_start = i + 1
    elif ch == ';':
      header_start = i + 1
  for region in regions:
    if region.end < 0:
      region.end = len(masked) - 1
  return regions


def _innermost(regions, offset, kinds):
  best = None
  for region in regions:
    if region.kind in kinds and region.start < offset < region.end:
      if best is None or region.start > best.start:
        best = region
  return best


def _statement_bounds(masked: str, offset: int):
  depth, i = 0, offset
  while i > 0:
    ch = masked[i - 1]
    if ch in ')]':
      depth += 1
    elif ch in '([':
      if depth == 0:
        pass
      else:
        depth -= 1
    elif ch in ';{}' and depth == 0:
      break
    i -= 1
  start = i
  depth, j = 0, offset
  while j < len(masked):
    ch = masked[j]
    if ch in '([{':
      depth += 1
    elif ch in ')]}':
      if depth == 0:
        break
      depth -= 1
    elif ch == ';' and depth == 0:
      break
    j += 1
  return start, j


def _first_statement_is_skip(code: str, region: _Region) -> bool:
  body = code[region.start + 1:region.end].lstrip()
  return body.startswith('GTEST_SKIP')


_re_code_literal = re.compile(r'^(?:::)?(?:hlc::)?(?:ErrorDefinition::)?(?:ErrorType::)?([A-Z][A-Z0-9_]*)$')
_re_string_literal = re.compile(r'^(?:std::string(?:_view)?\s*[({]\s*)?"((?:[^"\\]|\\.)*)"\s*[)}]?$')
_re_int_literal = re.compile(r'^(\d+)[uU]?$')


def _parse_call_args(args, codes):
  """findError(type[, symbol][, line[, col]]) -> (code or None, code_arg, symbol, line, col)"""
  if not args:
    return None, '', None, 0, 0
  m = _re_code_literal.match(args[0].replace(' ', ''))
  code = m.group(1) if m and m.group(1) in codes else None
  symbol, line, col = None, 0, 0
  rest = args[1:]
  if rest:
    sm = _re_string_literal.match(rest[0])
    im = _re_int_literal.match(rest[0])
    if sm:
      symbol = sm.group(1)
      rest = rest[1:]
    elif im:
      pass
    else:
      vm = re.match(r'^(\w+)$', rest[0])
      symbol = ('var', vm.group(1)) if vm else ('expr', rest[0])
      rest = rest[1:]
  if rest:
    im = _re_int_literal.match(rest[0])
    line = int(im.group(1)) if im else -1
  if len(rest) > 1:
    im = _re_int_literal.match(rest[1])
    col = int(im.group(1)) if im else -1
  return code, args[0], symbol, line, col


def _truth_of_expr(expr: str, call_span):
  """Given an expression text containing a findError call, returns True if the expression
  is truthy when the error is found, False if truthy when not found, None if unknown."""
  s, e = call_span
  after = expr[e:].lstrip()
  before = expr[:s].rstrip()
  negated = before.endswith('!') and not before.endswith('!=')
  if after.startswith('!= nullptr') or after.startswith('!=nullptr'):
    return not negated
  if after.startswith('== nullptr') or after.startswith('==nullptr'):
    return negated
  if before.endswith('nullptr !=') or before.endswith('nullptr!='):
    return True
  if before.endswith('nullptr ==') or before.endswith('nullptr=='):
    return False
  return not negated


def _polarity_from_assert(stmt: str, needle_span):
  """stmt contains the needle (a findError call or a variable) inside an assertion macro.
  Returns 'present'/'absent'/None."""
  m = re.match(r'\s*(ASSERT|EXPECT)_(NE|EQ|TRUE|FALSE)\s*\(', stmt)
  if not m:
    return None
  kind = m.group(2)
  if kind in ('NE', 'EQ'):
    inner = stmt[m.end():]
    if 'nullptr' not in inner and 'NULL' not in inner:
      return None
    return 'present' if kind == 'NE' else 'absent'
  truth = _truth_of_expr(stmt, needle_span)
  if truth is None:
    return None
  found_when_true = truth
  if kind == 'TRUE':
    return 'present' if found_when_true else 'absent'
  return 'absent' if found_when_true else 'present'


def _loop_values(code: str, region: _Region, offset: int, var: str):
  """If 'var' is the variable of an enclosing range-for over a braced list of string literals,
  returns those literals; otherwise None."""
  found = None
  pattern = r'for\s*\(\s*(?:const\s+)?[\w:<>]+\s*[&*]?\s*(?:const\s+)?(\w+)\s*:\s*\{([^{}]*)\}\s*\)'
  for m in re.finditer(pattern, code[region.start:offset]):
    if m.group(1) == var:
      found = m
  if found is None:
    return None
  values = []
  for item in found.group(2).split(','):
    item = item.strip()
    if not item:
      continue
    sm = _re_string_literal.match(item)
    if not sm:
      return None
    values.append(sm.group(1))
  return values or None


def _var_polarity(code: str, masked: str, region: _Region, var: str, after: int, var_truth: bool):
  """Finds an assertion on 'var' after offset 'after' within region. var_truth: var is truthy
  when the error is found. Returns (polarity or None, conditional_skip, group_key)."""
  meaning, cond_skip, group_key = _var_polarity_impl(code, region, var, after)
  if meaning is None:
    return None, False, None
  if meaning in ('nonnull', 'truthy'):
    return ('present' if var_truth else 'absent'), cond_skip, group_key
  return ('absent' if var_truth else 'present'), cond_skip, group_key


def _var_polarity_impl(code: str, region: _Region, var: str, after: int):
  body = code[after:region.end]
  vre = re.escape(var)
  skip_pats = [
    (rf'if\s*\(\s*{vre}\s*==\s*nullptr\s*\)\s*\{{?\s*GTEST_SKIP', 'nonnull'),
    (rf'if\s*\(\s*!\s*{vre}\s*\)\s*\{{?\s*GTEST_SKIP', 'truthy'),
    (rf'if\s*\(\s*{vre}\s*!=\s*nullptr\s*\)\s*\{{?\s*GTEST_SKIP', 'null'),
    (rf'if\s*\(\s*{vre}\s*\)\s*\{{?\s*GTEST_SKIP', 'falsy'),
  ]
  pats = [(p, m, True) for p, m in skip_pats] + [(p, m, False) for p, m in [
    (rf'(ASSERT|EXPECT)_NE\s*\(\s*{vre}\s*,\s*(nullptr|NULL)\s*\)|(ASSERT|EXPECT)_NE\s*\(\s*(nullptr|NULL)\s*,\s*{vre}\s*\)', 'nonnull'),
    (rf'(ASSERT|EXPECT)_EQ\s*\(\s*{vre}\s*,\s*(nullptr|NULL)\s*\)|(ASSERT|EXPECT)_EQ\s*\(\s*(nullptr|NULL)\s*,\s*{vre}\s*\)', 'null'),
    (rf'(ASSERT|EXPECT)_TRUE\s*\(\s*{vre}\s*(!=\s*nullptr\s*)?\)', 'truthy'),
    (rf'(ASSERT|EXPECT)_FALSE\s*\(\s*{vre}\s*(!=\s*nullptr\s*)?\)', 'falsy'),
    (rf'(ASSERT|EXPECT)_TRUE\s*\(\s*{vre}\s*==\s*nullptr\s*\)', 'falsy'),
    (rf'(ASSERT|EXPECT)_FALSE\s*\(\s*{vre}\s*==\s*nullptr\s*\)', 'truthy'),
    (rf'(ASSERT|EXPECT)_TRUE\s*\(\s*!\s*{vre}\s*\)', 'falsy'),
    (rf'(ASSERT|EXPECT)_FALSE\s*\(\s*!\s*{vre}\s*\)', 'truthy'),
    (rf'if\s*\(\s*{vre}\s*==\s*nullptr\s*\)\s*\{{?\s*(return|FAIL|ADD_FAILURE)', 'nonnull'),
    (rf'if\s*\(\s*!\s*{vre}\s*\)\s*\{{?\s*(return|FAIL|ADD_FAILURE)', 'truthy'),
  ]]
  best = None
  for pat, meaning, skip in pats:
    m = re.search(pat, body)
    if m and (best is None or m.start() < best[0]):
      best = (m.start(), meaning, skip, None)

  # EXPECT_TRUE(a || b) / EXPECT_FALSE(a || b): a boolean combination of variables.
  for m in re.finditer(r'(ASSERT|EXPECT)_(TRUE|FALSE)\s*\(', body):
    if best is not None and m.start() > best[0]:
      break
    close = _match_close(body, m.end() - 1, '(', ')')
    inner = body[m.end():close]
    if not re.search(rf'(?<![\w.>]){vre}\b', inner):
      continue
    if not re.fullmatch(r'[\s\w!|&()]*', inner):
      continue
    negated = re.search(rf'!\s*{vre}\b', inner) is not None
    truthy = (m.group(2) == 'TRUE') != negated
    group_key = after + m.start() if ('||' in inner and truthy) else None
    best = (m.start(), 'truthy' if truthy else 'falsy', False, group_key)
    break

  if best is None:
    # A dereference (err->getLocations()) without a guard implies the test expects it present.
    if re.search(rf'\b{vre}\s*->', body):
      return 'nonnull', False, None
    return None, False, None
  return best[1], best[2], best[3]


def _scan_cpp(args):
  test_ids, cpp_filepath, codes = args
  text = Path(cpp_filepath).read_text(encoding='utf-8', errors='replace')
  code, masked = _mask_cpp(text)
  regions = _find_regions(masked)
  line_starts = [0] + [i + 1 for i, ch in enumerate(text) if ch == '\n']

  def lineno(offset):
    lo, hi = 0, len(line_starts)
    while lo + 1 < hi:
      mid = (lo + hi) // 2
      if line_starts[mid] <= offset:
        lo = mid
      else:
        hi = mid
    return lo + 1

  fixture_skipped = set()
  for region in regions:
    if region.kind == 'function' and region.name in ('SetUpTestSuite', 'SetUp', 'SetUpTestCase'):
      if _first_statement_is_skip(code, region):
        cls = _innermost(regions, region.start, ('class',))
        if cls:
          fixture_skipped.add(cls.name)

  skip_points = defaultdict(list)    # test region start -> offsets of unconditional GTEST_SKIP
  for m in re.finditer(r'\bGTEST_SKIP\s*\(', masked):
    inner = _innermost(regions, m.start(), ('test', 'function', 'class', 'block'))
    if inner is not None and inner.kind == 'test':
      s0, _ = _statement_bounds(masked, m.start())
      if masked[s0:m.start()].strip() == '':
        skip_points[inner.start].append(m.start())

  def is_skipped(test_region, offset=None):
    if test_region is None:
      return False
    if offset is not None and any(p < offset for p in skip_points.get(test_region.start, [])):
      return True
    if test_region.fixture in fixture_skipped:
      return True
    if '.DISABLED_' in '.' + test_region.name.split('.', 1)[-1] or test_region.name.split('.', 1)[-1].startswith('DISABLED_'):
      return True
    return _first_statement_is_skip(code, test_region)

  # (helper name) -> (polarity, symbol param index, code is list) for helpers that call
  # findError with a non-literal code.
  helpers = {}
  calls = []     # (offset, end, args)
  for m in re.finditer(r'\bfindError\s*\(', masked):
    open_paren = m.end() - 1
    close = _match_close(masked, open_paren, '(', ')')
    calls.append((m.start(), close + 1, _split_args(code, masked, open_paren + 1, close)))

  raw = []       # dicts before group/aid assignment
  unparsed = []
  group_counter = [0]

  def new_group():
    group_counter[0] += 1
    return group_counter[0]

  var_groups = {}   # (region start, var) -> group id

  for start, end, cargs in calls:
    test_region = _innermost(regions, start, ('test',))
    func_region = _innermost(regions, start, ('test', 'function'))
    if func_region is None:
      continue
    code_name, code_arg, symbol, line, col = _parse_call_args(cargs, codes)
    s0, s1 = _statement_bounds(masked, start)
    stmt = code[s0:s1]
    span = (start - s0, end - s0)
    cond_region = _innermost(regions, start, ('cond',))
    conditional = (cond_region is not None and cond_region.start > func_region.start) or \
      re.match(r'\s*(if|else)\b', stmt) is not None
    polarity = None
    source = 'direct'
    group = 0

    if code_name is None:
      # Non-literal code: a helper taking the code as a parameter.
      if func_region.kind == 'function':
        pol = _polarity_from_assert(stmt, span)
        if pol is None:
          vm = re.match(r'\s*(?:[\w:<>,\s*&]+?\s+)?(\w+)\s*=\s*', stmt)
          if vm and stmt.find('findError') > vm.end() - 1:
            pol = _var_polarity(code, masked, func_region, vm.group(1), s1, _truth_of_expr(stmt, span))[0]
        helpers[func_region.name] = pol
        continue
      unparsed.append(Unparsed('', cpp_filepath, lineno(start), stmt.strip()[:200], f'non-literal code "{code_arg}"'))
      continue

    polarity = _polarity_from_assert(stmt, span)
    cond_skip = False
    if polarity is None:
      vm = re.match(r'\s*(?:(?:const|static|auto|bool|[\w:<>]+)\s*[*&]?\s*(?:const\s+)?)*?(\w+)\s*=\s*(?!=)', stmt)
      if vm and stmt.find('findError') >= vm.end():
        var = vm.group(1)
        truth = _truth_of_expr(stmt, span)
        polarity, cond_skip, group_key = _var_polarity(code, masked, func_region, var, s1, truth)
        source = 'variable'
        key = (func_region.start, 'assert', group_key) if group_key is not None else (func_region.start, var)
        if key in var_groups:
          group = var_groups[key]
        else:
          var_groups[key] = group = new_group()
      else:
        # An 'if (err == nullptr) err = findError(...)' reassignment chain.
        vm2 = re.search(r'(\w+)\s*=\s*$', code[s0:start])
        if vm2:
          key = (func_region.start, vm2.group(1))
          group = var_groups.get(key, 0) or new_group()
          var_groups[key] = group
          polarity, cond_skip, _ = _var_polarity(code, masked, func_region, vm2.group(1), s1, True)
          source = 'variable'

    if polarity is None:
      unparsed.append(Unparsed('', cpp_filepath, lineno(start), stmt.strip()[:200], 'polarity not determined'))
      continue

    # Several findError calls joined by || in one expression form an any-of group when present.
    if group == 0 and stmt.count('findError') > 1 and '||' in stmt and polarity == 'present':
      key = (func_region.start, s0)
      group = var_groups.get(key) or new_group()
      var_groups[key] = group

    # A non-literal symbol: expand a range-for over string literals, otherwise give up.
    symbols = [symbol]
    if isinstance(symbol, tuple):
      values = _loop_values(code, func_region, start, symbol[1]) if symbol[0] == 'var' else None
      if values is None:
        reason = ('symbol is a helper parameter' if func_region.kind == 'function'
                  else f'non-literal symbol "{symbol[1]}"')
        unparsed.append(Unparsed('', cpp_filepath, lineno(start), stmt.strip()[:200], reason))
        continue
      symbols = values

    for sym in symbols:
      if func_region.kind == 'function':
        # A literal findError inside a fixture helper: credit it to every caller, polarity as found.
        raw.append(dict(cpp_line=lineno(start), test_name=f'{func_region.name}()', code=code_name, symbol=sym,
                        line=max(line, 0), col=max(col, 0), polarity=polarity, skipped=cond_skip, source='helper',
                        group=group, helper=func_region.name, conditional=conditional))
      else:
        raw.append(dict(cpp_line=lineno(start), test_name=test_region.name if test_region else func_region.name,
                        code=code_name, symbol=sym, line=max(line, 0), col=max(col, 0), polarity=polarity,
                        skipped=is_skipped(test_region, start) or cond_skip, source=source, group=group, helper=None,
                        conditional=conditional))

  # Resolve helpers: a literal-code helper's assertion is attributed to each TEST that calls it.
  literal_helpers = defaultdict(list)
  for item in raw:
    if item['helper']:
      literal_helpers[item['helper']].append(item)
  final = [item for item in raw if not item['helper']]

  for region in regions:
    if region.kind != 'test':
      continue
    body_code = code[region.start:region.end]
    body_mask = masked[region.start:region.end]
    for name, items in literal_helpers.items():
      hm = re.search(rf'\b{re.escape(name)}\s*\(', body_mask)
      if hm:
        for item in items:
          clone = dict(item)
          clone['test_name'] = region.name
          clone['skipped'] = clone['skipped'] or is_skipped(region, region.start + hm.start())
          clone['helper'] = None
          final.append(clone)
    for name, pol in helpers.items():
      for m in re.finditer(rf'\b{re.escape(name)}\s*\(', body_mask):
        open_paren = m.end() - 1
        close = _match_close(body_mask, open_paren, '(', ')')
        call_text = body_code[open_paren + 1:close]
        call_codes = [c for c in re.findall(r'(?:ErrorDefinition::)?(?:ErrorType::)?\b([A-Z][A-Z0-9_]{3,})\b', call_text) if c in codes]
        if not call_codes:
          continue
        if pol is None:
          unparsed.append(Unparsed('', cpp_filepath, lineno(region.start + m.start()), body_code[m.start():close + 1].strip()[:200],
                                   f'helper {name}() polarity not determined'))
          continue
        call_args = _split_args(body_code, body_mask, open_paren + 1, close)
        is_name = lambda a: re.fullmatch(r'\w+', a) and not re.fullmatch(r'[A-Z][A-Z0-9_]*|\d+|true|false|nullptr', a)
        symbols = None
        for arg in call_args:
          sm = _re_string_literal.match(arg)
          if sm:
            symbols = [sm.group(1)]
            break
        if symbols is None:
          names = [a for a in call_args if is_name(a)]
          if names:
            symbols = _loop_values(code, region, region.start + m.start(), names[0])
            if symbols is None:
              unparsed.append(Unparsed('', cpp_filepath, lineno(region.start + m.start()),
                                       body_code[m.start():close + 1].strip()[:200], f'helper {name}() symbol not resolved'))
              continue
        for sym in symbols or [None]:
          group = new_group() if len(call_codes) > 1 and pol == 'present' else 0
          for c in call_codes:
            final.append(dict(cpp_line=lineno(region.start + m.start()), test_name=region.name, code=c,
                              symbol=sym, line=0, col=0, polarity=pol,
                              skipped=is_skipped(region, region.start + m.start()), source='helper', group=group,
                              helper=None, conditional=False))

  for item in unparsed:
    item.test_id = ','.join(test_ids)
  return test_ids, cpp_filepath, final, unparsed


def _link_cpp(test_dirpath: Path, tests: dict[str, TestCase]):
  by_hlc = {t.hlc_filepath.resolve(): t for t in tests.values()}
  links = []
  for cpp in sorted(test_dirpath.rglob('test_*.cpp')):
    text = cpp.read_text(encoding='utf-8', errors='replace')
    targets = []
    for name in re.findall(r'"([^"]+\.hlc)"', text):
      test = by_hlc.get((cpp.parent / name).resolve())
      if test:
        targets.append(test.test_id)
    if not targets:
      for hlc in cpp.parent.glob('*.hlc'):
        test = by_hlc.get(hlc.resolve())
        if test:
          targets.append(test.test_id)
    if targets:
      for test_id in targets:
        tests[test_id].cpp_filepaths.append(cpp)
      links.append((targets, cpp))
  return links


# --------------------------------------------------------------------------------------------------
# Join
# --------------------------------------------------------------------------------------------------

def _matches(assertion: Assertion, report: Report) -> bool:
  if assertion.code != report.code:
    return False
  if assertion.symbol is not None and assertion.symbol != report.symbol:
    return False
  if assertion.line > 0:
    locations = [(report.line, report.col)] + report.extra_locations
    if not any(l == assertion.line and (assertion.col <= 0 or c == assertion.col) for l, c in locations):
      return False
  return True


def _join(tests, reports_by_test, assertions_by_test):
  for test_id in tests:
    reports = reports_by_test.get(test_id, [])
    assertions = assertions_by_test.get(test_id, [])

    for a in assertions:
      for r in reports:
        if _matches(a, r):
          a.matched.append(r)
          r.matched_by.append(a)

    satisfied_groups = {a.group for a in assertions if a.group and a.polarity == 'present' and a.matched}

    for a in assertions:
      if a.polarity == 'present':
        if a.matched:
          a.status = 'Satisfied (skipped)' if a.skipped else 'Checked'
        elif a.group and a.group in satisfied_groups:
          a.status = 'Alternative'
        elif a.skipped:
          a.status = 'Pending'
        else:
          a.status = 'Conditional' if a.conditional else 'Broken'
      elif a.skipped:
        # A skipped expect-absent tracks a known bogus diagnostic: still reported -> pending,
        # gone -> the skip can be removed.
        a.status = 'Pending' if a.matched else 'Satisfied (skipped)'
      elif a.matched:
        a.status = 'Conditional' if a.conditional else 'Contradiction'
      else:
        a.status = 'Guard'

    for r in reports:
      live = [a for a in r.matched_by if not a.skipped]
      if any(a.polarity == 'absent' and not a.conditional for a in live):
        r.status = 'Contradiction'
      elif any(a.polarity == 'present' for a in live):
        r.status = 'Checked'
      elif any(a.polarity == 'present' for a in r.matched_by):
        r.status = 'Satisfied (skipped)'
      elif any(a.polarity == 'absent' for a in live):
        r.status = 'Conditional'
      elif r.matched_by:
        r.status = 'Known bogus'
      else:
        r.status = 'Not checked'


def _code_buckets(codes, all_reports, all_assertions):
  stats = {}
  for name, code in codes.items():
    stats[name] = dict(code=code, local=0, local_tests=set(), external=0, external_tests=set(),
                       present=0, present_matched=0, present_skipped=0, absent=0, tests_example=[])

  def get(name):
    if name not in stats:
      stats[name] = dict(code=None, local=0, local_tests=set(), external=0, external_tests=set(),
                         present=0, present_matched=0, present_skipped=0, absent=0, tests_example=[])
    return stats[name]

  for r in all_reports:
    s = get(r.code)
    if r.scope == 'local':
      s['local'] += 1
      s['local_tests'].add(r.test_id)
    else:
      s['external'] += 1
      s['external_tests'].add(r.test_id)

  for a in all_assertions:
    s = get(a.code)
    if a.polarity == 'present':
      if a.skipped:
        s['present_skipped'] += 1
      else:
        s['present'] += 1
        if a.matched:
          s['present_matched'] += 1
    else:
      s['absent'] += 1

  for s in stats.values():
    if s['present_matched']:
      bucket = 'Checked'
    elif s['local'] or s['external']:
      bucket = 'Not checked'
    elif s['present'] or s['present_skipped']:
      bucket = 'Pending'
    elif s['absent']:
      bucket = 'Guard-only'
    else:
      bucket = 'Dead'
    s['bucket'] = bucket
    s['tests_example'] = sorted(s['local_tests'] or s['external_tests'])[:5]
  return stats


# --------------------------------------------------------------------------------------------------
# Output
# --------------------------------------------------------------------------------------------------

def _code_label(stats_entry, name):
  code = stats_entry['code']
  if code is None:
    return name.lstrip('?'), ''
  return f'{code.number:04d}', code.category


def _write_xlsx(filepath, codes, tests, stats, reports_by_test, assertions_by_test, unparsed, missing_logs, summary_rows):
  from openpyxl import Workbook
  from openpyxl.styles import Font, PatternFill
  from openpyxl.utils import get_column_letter

  wb = Workbook(write_only=True)
  bold = Font(bold=True)
  fills = {
    'Checked': PatternFill('solid', fgColor='C6EFCE'),
    'All checked': PatternFill('solid', fgColor='C6EFCE'),
    'Partly checked': PatternFill('solid', fgColor='FFEB9C'),
    'No cpp': PatternFill('solid', fgColor='FFEB9C'),
    'Problem': PatternFill('solid', fgColor='FFC7CE'),
    'Not checked': PatternFill('solid', fgColor='FFEB9C'),
    'Contradiction': PatternFill('solid', fgColor='FFC7CE'),
    'Broken': PatternFill('solid', fgColor='FFC7CE'),
    'Pending': PatternFill('solid', fgColor='DDEBF7'),
    'Conditional': PatternFill('solid', fgColor='FCE4D6'),
    'Satisfied (skipped)': PatternFill('solid', fgColor='E4DFEC'),
    'Dead': PatternFill('solid', fgColor='D9D9D9'),
  }

  from openpyxl.cell.cell import ILLEGAL_CHARACTERS_RE

  def clean(value):
    if isinstance(value, str):
      value = ILLEGAL_CHARACTERS_RE.sub('?', value)
      return value[:32000]
    return value

  def sheet(title, headers, rows, widths=None, status_col=None):
    rows = [[clean(v) for v in row] for row in rows]
    ws = wb.create_sheet(title)
    ws.freeze_panes = 'A2'
    for i, w in enumerate(widths or []):
      ws.column_dimensions[get_column_letter(i + 1)].width = w
    from openpyxl.cell import WriteOnlyCell
    header_cells = []
    for h in headers:
      c = WriteOnlyCell(ws, value=h)
      c.font = bold
      header_cells.append(c)
    ws.append(header_cells)
    for row in rows:
      if status_col is not None and row[status_col] in fills:
        cells = []
        for i, v in enumerate(row):
          c = WriteOnlyCell(ws, value=v)
          if i == status_col:
            c.fill = fills[row[status_col]]
          cells.append(c)
        ws.append(cells)
      else:
        ws.append(list(row))
    if rows:
      ws.auto_filter.ref = f'A1:{get_column_letter(len(headers))}{len(rows) + 1}'

  # Summary
  sheet('Summary', ['Metric', 'Value'], summary_rows, [48, 90])

  # By code
  rows = []
  for name, s in sorted(stats.items(), key=lambda kv: (['Checked', 'Not checked', 'Pending', 'Guard-only', 'Dead'].index(kv[1]['bucket']), kv[0])):
    code = s['code']
    if code is None:
      # An id that is not in ErrorDefinition.h comes from a stale log; it is listed under Problems.
      continue
    rows.append([
      name,
      code.number if code else name.lstrip('?'),
      code.category if code else '(unknown)',
      code.severity if code else '',
      code.text if code else '',
      s['bucket'],
      s['local'], len(s['local_tests']),
      s['external'], len(s['external_tests']),
      s['present'], s['present_matched'], s['present_skipped'], s['absent'],
      ', '.join(s['tests_example']),
    ])
  sheet('By code', ['Code', 'Number', 'Category', 'Severity', 'Message', 'Bucket',
                    'Local reports', 'Local tests', 'External reports', 'External tests',
                    'Expect-present asserts', 'Expect-present matched', 'Expect-present skipped', 'Expect-absent asserts',
                    'Example tests'],
        rows, [44, 9, 9, 9, 60, 14, 12, 11, 14, 13, 14, 14, 14, 14, 80], status_col=5)

  # By test: one row per test with its coverage and the codes in each state.
  def code_list(items):
    counts = defaultdict(int)
    for item in items:
      counts[item.code] += 1
    return ', '.join(f'{c} ({n})' if n > 1 else c for c, n in sorted(counts.items()))

  rows = []
  for test_id, test in tests.items():
    main = [r for r in reports_by_test.get(test_id, [])
            if r.scope == 'local' and r.severity in _main_severities]
    assertions = assertions_by_test.get(test_id, [])
    checked = [r for r in main if r.status == 'Checked']
    not_checked = [r for r in main if r.status != 'Checked']
    missing = [a for a in assertions if a.status in ('Pending', 'Broken')]
    problem = any(r.status == 'Contradiction' for r in main) or any(a.status == 'Broken' for a in assertions)
    if test_id in missing_logs:
      status = 'No log'
    elif problem:
      status = 'Problem'
    elif not main:
      status = 'No errors reported'
    elif not test.cpp_filepaths:
      status = 'No cpp'
    elif not not_checked:
      status = 'All checked'
    elif checked:
      status = 'Partly checked'
    else:
      status = 'Not checked'
    rows.append([
      test_id, 'yes' if test.cpp_filepaths else 'no', status,
      round(100.0 * len(checked) / len(main), 1) if main else None,
      len(main), len(checked), len(not_checked),
      code_list(not_checked), code_list(missing),
    ])
  sheet('By test', ['Test', 'Has cpp', 'Status', 'Coverage %', 'Reported', 'Checked', 'Not checked',
                    'Reported codes not checked', 'Expected but missing'],
        rows, [60, 8, 18, 11, 10, 9, 11, 70, 50], status_col=2)

  # Problems
  rows = []
  for test_id in tests:
    for r in reports_by_test.get(test_id, []):
      if r.status == 'Contradiction':
        a = next(a for a in r.matched_by if a.polarity == 'absent' and not a.skipped)
        rows.append(['Contradiction', test_id, r.code, f'{r.path}:{r.line}:{r.col}',
                     f'{Path(a.cpp_filepath).name}:{a.cpp_line} {a.test_name}', 'Reported, but the test asserts it is absent'])
    stale_log = any(r.code.startswith('?') for r in reports_by_test.get(test_id, []))
    for a in assertions_by_test.get(test_id, []):
      if a.status == 'Conditional':
        rows.append(['Conditional', test_id, a.code, '', f'{Path(a.cpp_filepath).name}:{a.cpp_line} {a.test_name}',
                     ('Expect-absent check inside an if/else does not hold against this log'
                      if a.polarity == 'absent' else
                      'Expect-present check inside an if/else is not in this log') +
                     ' (the guarding condition may not be true in the gtest run)'])
      if a.status == 'Broken':
        rows.append(['Broken', test_id, a.code, '', f'{Path(a.cpp_filepath).name}:{a.cpp_line} {a.test_name}',
                     'Asserted present (not skipped), but not in the log' +
                     (' -- the log uses pre-renumbering ids, regenerate it first' if stale_log else '')])
    for (cpp, test_name), codes_ in _stale_skip_candidates(assertions_by_test.get(test_id, [])):
      rows.append(['Stale skip candidate', test_id, ', '.join(codes_), '', f'{Path(cpp).name} {test_name}',
                   'Skipped, but every findError check in it already holds; check its other assertions and un-skip'])
    unknown = defaultdict(int)
    for r in reports_by_test.get(test_id, []):
      if r.code.startswith('?'):
        unknown[r.code] += 1
    for code_name, count in sorted(unknown.items()):
      rows.append(['Unknown code', test_id, code_name, '', '',
                   f'{count} line(s); id not in ErrorDefinition.h (log likely predates the code renumbering)'])
  for u in unparsed:
    rows.append(['Unparsed findError', u.test_id, '', '', f'{Path(u.cpp_filepath).name}:{u.cpp_line}', f'{u.reason}: {u.context}'])
  for test_id in sorted(missing_logs):
    rows.append(['Missing log', test_id, '', '', '', 'No <name>.log for this test'])
  sheet('Problems', ['Kind', 'Test', 'Code', 'Location', 'Cpp', 'Detail'], rows, [18, 55, 40, 50, 55, 90])

  order = ['Summary', 'By test', 'By code', 'Problems']
  wb._sheets.sort(key=lambda ws: order.index(ws.title) if ws.title in order else len(order))
  wb.save(filepath)


def _stale_skip_candidates(assertions):
  by_test = defaultdict(list)
  for a in assertions:
    by_test[(a.cpp_filepath, a.test_name)].append(a)
  result = []
  for key, items in by_test.items():
    if not any(a.skipped for a in items):
      continue
    if all(a.status in ('Checked', 'Guard', 'Satisfied (skipped)', 'Alternative', 'Conditional') for a in items):
      if any(a.status == 'Satisfied (skipped)' for a in items):
        result.append((key, sorted({a.code for a in items})))
  return result


def _pct(n, d):
  return f'{(100.0 * n / d):.1f}%' if d else '-'


def _build_summary(mode, log_dirpath, codes, tests, stats, reports_by_test, assertions_by_test, unparsed, missing_logs):
  buckets = defaultdict(int)
  for name, s in stats.items():
    if s['code'] is not None:
      buckets[s['bucket']] += 1

  local = [r for rs in reports_by_test.values() for r in rs if r.scope == 'local']
  main = [r for r in local if r.severity in _main_severities]
  notes = [r for r in local if r.severity in _note_severities]
  external = [r for rs in reports_by_test.values() for r in rs if r.scope == 'external']
  assertions = [a for as_ in assertions_by_test.values() for a in as_]
  by_status = lambda items: defaultdict(int, ((st, sum(1 for x in items if x.status == st)) for st in {x.status for x in items}))
  ms, ns, ast = by_status(main), by_status(notes), by_status(assertions)

  with_cpp = sum(1 for t in tests.values() if t.cpp_filepaths)
  no_cpp_reporting = sum(1 for t in tests.values()
                         if not t.cpp_filepaths and any(r.scope == 'local' and r.severity in _main_severities
                                                        for r in reports_by_test.get(t.test_id, [])))
  unknown = sum(1 for r in local + external if r.code.startswith('?'))
  stale = sum(len(_stale_skip_candidates(v)) for v in assertions_by_test.values())

  summary = [
    ('Mode', mode),
    ('Logs', str(log_dirpath)),
    ('Severities (main)', ', '.join(_main_severities) + '  (NTE counted separately, INF excluded)'),
    ('Error codes (ErrorDefinition.h)', len(codes)),
    ('  Checked', buckets['Checked']),
    ('  Not checked (reported, no test checks it)', buckets['Not checked']),
    ('  Pending', buckets['Pending']),
    ('  Guard-only', buckets['Guard-only']),
    ('  Dead', buckets['Dead']),
    ('Tests', len(tests)),
    ('  With cpp', with_cpp),
    ('  Without cpp but reporting errors', no_cpp_reporting),
    ('  Missing log', len(missing_logs)),
    ('Local diagnostics (main)', len(main)),
    ('  Checked', f'{ms["Checked"]} ({_pct(ms["Checked"], len(main))})'),
    ('  Not checked', ms['Not checked']),
    ('  Satisfied while skipped', ms['Satisfied (skipped)']),
    ('  Contradiction', ms['Contradiction']),
    ('Local notes (NTE)', f'{len(notes)} (checked {ns["Checked"]}, not checked {ns["Not checked"]})'),
    ('External diagnostics (excluded)', len(external)),
    ('Assertions (findError)', len(assertions)),
    ('  Checked', ast['Checked']),
    ('  Pending (skipped)', ast['Pending']),
    ('  Broken', ast['Broken']),
    ('  Alternative', ast['Alternative']),
    ('  Guard', ast['Guard']),
    ('  Contradiction', ast['Contradiction']),
    ('  Conditional (inside if/else, does not hold here)', ast['Conditional']),
    ('  Satisfied while skipped', ast['Satisfied (skipped)']),
    ('  Stale skip candidates (TESTs)', stale),
    ('  Unparsed findError calls', len(unparsed)),
    ('Unknown codes in logs', unknown),
  ]

  top = sorted(((name, s) for name, s in stats.items() if s['bucket'] == 'Not checked' and s['local']),
               key=lambda kv: (-len(kv[1]['local_tests']), kv[0]))[:20]

  lines = ['## Error Coverage', '',
           f'Mode: **{mode}** - local diagnostics, {"/".join(_main_severities)} (NTE separate, INF and library sources excluded)', '',
           '| Codes | Checked | Not checked | Pending | Guard-only | Dead |',
           '|---:|---:|---:|---:|---:|---:|',
           f'| {len(codes)} | {buckets["Checked"]} | {buckets["Not checked"]} | {buckets["Pending"]} | {buckets["Guard-only"]} | {buckets["Dead"]} |',
           '',
           '| Reported | Checked | Not checked | Checked by a skipped test | Contradiction | Notes (NTE) |',
           '|---:|---:|---:|---:|---:|---:|',
           f'| {len(main)} | {ms["Checked"]} ({_pct(ms["Checked"], len(main))}) | {ms["Not checked"]} | {ms["Satisfied (skipped)"]} | {ms["Contradiction"]} | {len(notes)} |',
           '',
           '| Tests | With cpp | No cpp but reporting | Missing log |',
           '|---:|---:|---:|---:|',
           f'| {len(tests)} | {with_cpp} | {no_cpp_reporting} | {len(missing_logs)} |',
           '',
           '| Checks | Matched | Pending | Broken | Guard | Contradiction | Stale-skip TESTs | Unparsed |',
           '|---:|---:|---:|---:|---:|---:|---:|---:|',
           f'| {len(assertions)} | {ast["Checked"]} | {ast["Pending"]} | {ast["Broken"]} | {ast["Guard"]} | {ast["Contradiction"]} | {stale} | {len(unparsed)} |',
           '']
  if top:
    lines += ['### Top codes reported but not checked (by tests)', '',
              '| Code | Id | Severity | Tests | Times reported |', '|---|---|---|---:|---:|']
    for name, s in top:
      c = s['code']
      code_id = f'{_category_prefixes.get(c.category, "")}{c.number:04d}' if c else ''
      lines.append(f'| {name} | {code_id} | {c.severity if c else ""} | {len(s["local_tests"])} | {s["local"]} |')
    lines.append('')
  return summary, '\n'.join(lines)


# --------------------------------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------------------------------

def _main():
  parser = argparse.ArgumentParser(description='Report which diagnostics davtests reports and asserts.')
  parser.add_argument('--hlc-repo', dest='hlc_repo', default=str(_workspace_dirpath.parent / 'hlc'),
                      help='Path to the hlc repository (for ErrorDefinition.h/.cpp).')
  parser.add_argument('--test-dirpath', dest='test_dirpath', default=str(_workspace_dirpath / 'hlc'),
                      help='Directory holding the .hlc files and test_*.cpp files.')
  parser.add_argument('--log-dirpath', dest='log_dirpath', default=None,
                      help='Directory holding <test_id>/<name>.log; defaults to --test-dirpath (goldens). '
                           'Point it at a regression output directory for fresh mode.')
  parser.add_argument('--regression-archives', dest='regression_archives', nargs='+', default=None,
                      help='Regression shard artifacts (Regression_*.zip, glob patterns allowed) to read the logs '
                           'from instead of --log-dirpath (fresh mode, as in CI).')
  parser.add_argument('--output-dirpath', dest='output_dirpath', default=str(_workspace_dirpath / 'out' / 'error_coverage'),
                      help='Where report.xlsx and summary.md are written.')
  parser.add_argument('--filter', default=None, help='Regex on test id to restrict the run.')
  parser.add_argument('--external-pattern', dest='external_pattern', default=_default_external_pattern,
                      help='Regex on a source path marking third-party library files.')
  parser.add_argument('--jobs', type=int, default=os.cpu_count() or 1, help='Worker processes.')
  args = parser.parse_args()

  hlc_repo = Path(args.hlc_repo).resolve()
  test_dirpath = Path(args.test_dirpath).resolve()
  log_dirpath = Path(args.log_dirpath).resolve() if args.log_dirpath else test_dirpath
  output_dirpath = Path(args.output_dirpath).resolve()
  output_dirpath.mkdir(parents=True, exist_ok=True)
  mode = 'golden' if log_dirpath == test_dirpath else 'fresh'
  filter_re = re.compile(args.filter) if args.filter else None

  if args.regression_archives:
    import glob
    archives = sorted({str(Path(p).resolve()) for pattern in args.regression_archives for p in glob.glob(pattern)})
    if not archives:
      raise FileNotFoundError(f'No regression archives match {args.regression_archives}')
    log_dirpath = output_dirpath / 'logs'
    log_dirpath.mkdir(parents=True, exist_ok=True)
    with multiprocessing.Pool(max(1, min(args.jobs, len(archives)))) as pool:
      for archive, count in pool.imap_unordered(_extract_regression_archive,
                                                [(a, str(log_dirpath)) for a in archives]):
        print(f'Extracted {count} logs from {Path(archive).name}', flush=True)
    mode = f'fresh ({len(archives)} regression shard(s))'

  codes = _load_code_table(hlc_repo)
  code_by_number = {c.number: c for c in codes.values()}
  print(f'Codes: {len(codes)}', flush=True)

  tests = _scan_tests(test_dirpath, filter_re)
  print(f'Tests: {len(tests)}', flush=True)

  links = _link_cpp(test_dirpath, tests)
  print(f'Cpp files: {len(links)}', flush=True)

  missing_logs = set()
  log_jobs = []
  for test in tests.values():
    log_filepath = _find_log(log_dirpath, test)
    if log_filepath is None:
      missing_logs.add(test.test_id)
    else:
      log_jobs.append((test, log_filepath, code_by_number, args.external_pattern, _workspace_dirpath))

  cpp_jobs = [(targets, str(cpp), set(codes)) for targets, cpp in links]

  reports_by_test = {}
  assertions_by_test = defaultdict(list)
  unparsed = []
  with multiprocessing.Pool(max(1, args.jobs)) as pool:
    for test_id, reports in pool.imap_unordered(_parse_log, log_jobs, chunksize=16):
      reports_by_test[test_id] = reports
    print(f'Logs parsed: {len(reports_by_test)}', flush=True)

    aid = 0
    for targets, cpp, items, bad in pool.imap_unordered(_scan_cpp, cpp_jobs, chunksize=8):
      unparsed.extend(bad)
      for item in items:
        for test_id in targets:
          aid += 1
          assertions_by_test[test_id].append(Assertion(
            aid=aid, test_id=test_id, cpp_filepath=cpp, cpp_line=item['cpp_line'], test_name=item['test_name'],
            code=item['code'], symbol=item['symbol'], line=item['line'], col=item['col'],
            polarity=item['polarity'], skipped=item['skipped'], source=item['source'],
            group=item['group'], conditional=item['conditional']))
  _regroup(assertions_by_test)
  print(f'Assertions: {sum(len(v) for v in assertions_by_test.values())}, unparsed: {len(unparsed)}', flush=True)

  _join(tests, reports_by_test, assertions_by_test)
  all_reports = [r for rs in reports_by_test.values() for r in rs]
  all_assertions = [a for as_ in assertions_by_test.values() for a in as_]
  stats = _code_buckets(codes, all_reports, all_assertions)

  summary_rows, summary_md = _build_summary(mode, log_dirpath, codes, tests, stats, reports_by_test,
                                            assertions_by_test, unparsed, missing_logs)
  (output_dirpath / 'summary.md').write_text(summary_md + '\n', encoding='utf-8')
  _write_xlsx(output_dirpath / 'report.xlsx', codes, tests, stats, reports_by_test, assertions_by_test,
              unparsed, missing_logs, summary_rows)

  step_summary = os.environ.get('GITHUB_STEP_SUMMARY')
  if step_summary:
    with open(step_summary, 'a', encoding='utf-8') as strm:
      strm.write(summary_md + '\n')

  print(summary_md)
  print(f'Wrote {output_dirpath / "report.xlsx"}')
  return 0


def _regroup(assertions_by_test):
  # Group ids are only unique within one cpp scan; renumber them per test so they cannot collide.
  for assertions in assertions_by_test.values():
    remap = {}
    for a in assertions:
      if a.group:
        original = (a.cpp_filepath, a.group)
        a.group = remap.setdefault(original, len(remap) + 1)


if __name__ == '__main__':
  sys.exit(_main())
