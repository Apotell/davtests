#!/usr/bin/env python3

"""
Per-event check of a regression run against the tests' test_*.cpp files.

For every test a regression ran, every event (FTL/SNT/ERR/WRN/NTE) in its log is matched against
the Test::findEvent() checks in the test's test_*.cpp, and the result is reported one event per
row. The checks a cpp makes that no event matches are reported too.

Input is one of:
  - regression.py's output directory, as in PR Verification (one <test_id>.tar.gz per test)
  - the Regression_*_PR_<n>.tar.gz of a PR run's artifact
  - a full hlc run's Summary artifact directory (events.log + regression.csv), to check every
    test at once. events.log keeps each event's continuation lines but has no NTE lines, so a
    check naming a note shows as unmatched.

The parsing and matching are event_coverage.py's, so the statuses agree with the full run's
Event Coverage report:

Event status:
  Checked              - an expect-present check in the cpp matches it
  Not checked          - no check in the cpp matches it (incl. tests without a cpp)
  Contradiction        - the cpp asserts it is absent
  Satisfied (skipped)  - only a skipped expect-present check matches it
  Known bogus          - only a skipped expect-absent check matches it (tracked HLC defect)
  Conditional          - only an expect-absent check inside an if/else matches it

Check problems (a cpp check no event satisfies, or a skip that no longer applies):
  Broken               - expect-present, not skipped, nothing logged (the test should fail)
  Pending              - skipped, and the event is still missing (or still wrongly reported)
  Satisfied (skipped)  - skipped, but it already holds -- the skip can likely be removed

Writes event_check.csv (every event and check problem, one per row) and event_check.md, which
is also appended to the GitHub job summary: one table, per test, of the cpp's checks (expect-
present, not skipped, an any-of group counted once), the log's events, and how many of those
checks found their event. The report never fails the job; ctest is what fails on a broken check.
"""

import argparse
import csv
import multiprocessing
import os
import re
import sys
import tarfile
from collections import defaultdict
from pathlib import Path

import event_coverage as ec
import log_parser
from event_coverage import _cpp_names, _location
from utils import build_filters

_this_dirpath = Path(__file__).parent.resolve()
_workspace_dirpath = _this_dirpath.parent

# Check statuses the CSV lists as a 'check' row.
_problem_statuses = ('Broken', 'Pending', 'Satisfied (skipped)')


_re_events_log_line = re.compile(r'^(?P<test_id>\S+): (?P<diag>\[(?:FTL|SNT|ERR|WRN):.*)$')
_re_events_log_continuation = re.compile(r'^(?P<test_id>\S+): (?P<cont>[ \t]+\S.*)$')


def _is_filtered(test_id: str, filters) -> bool:
  """regression.py's --filters syntax, anchored: a filter must match the whole test id or its last
  component (case-insensitive), so class_test_2 does not also keep class_test_20..29. No filters
  keep every test."""
  if not filters:
    return True
  name = test_id.rsplit('/', 1)[-1]
  for f in filters:
    if isinstance(f, str):
      if f.lower() in (test_id.lower(), name.lower()):
        return True
    elif f.fullmatch(test_id) or f.fullmatch(name):
      return True
  return False


def _collect_summary_logs(summary_dirpath: Path, out_dirpath: Path, filters) -> list[str]:
  """Splits a full run's Summary artifact (events.log + regression.csv, see summarize.py) into
  <out>/<test_id>/<name>.log. regression.csv names every test the run ran; events.log holds
  the FTL/SNT/ERR/WRN events, each header line followed by its indented continuation lines."""
  test_ids = []
  with open(summary_dirpath / 'regression.csv', encoding='utf-8') as strm:
    for line in strm:
      cells = [cell.strip() for cell in line.split('|')]
      if (len(cells) > 2) and cells[2] and (cells[2] != 'TESTNAME') and _is_filtered(cells[2], filters):
        test_ids.append(cells[2])

  lines_by_test = defaultdict(list)
  with open(summary_dirpath / 'events.log', encoding='utf-8', errors='replace') as strm:
    kept_test_id = None   # Test whose event the following continuation lines belong to.
    for line in strm:
      line = line.rstrip('\r\n')
      m = _re_events_log_line.match(line)
      if m:
        kept_test_id = m.group('test_id') if _is_filtered(m.group('test_id'), filters) else None
        if kept_test_id is not None:
          lines_by_test[kept_test_id].append(m.group('diag') + '\n')
        continue

      cm = _re_events_log_continuation.match(line)
      if cm and (cm.group('test_id') == kept_test_id):
        lines_by_test[kept_test_id].append(cm.group('cont') + '\n')
      else:
        kept_test_id = None   # e.g. an "Internal Error" header, whose lines are not checked.

  for test_id in set(test_ids) | set(lines_by_test):
    dirpath = out_dirpath / test_id
    dirpath.mkdir(parents=True, exist_ok=True)
    (dirpath / f'{Path(test_id).name}.log').write_text(''.join(lines_by_test.get(test_id, [])), encoding='utf-8')
  return sorted(set(test_ids) | set(lines_by_test), key=str.lower)


def _collect_logs(source: Path, out_dirpath: Path, filters, specs) -> list[str]:
  """Extracts the logs of a regression run into <out>/<test_id>/<name>.log and returns the test
  ids. source is regression.py's output directory (one <test_id>.tar.gz per test), the PR
  artifact's tarball of that directory (<top>/<test_id>.tar.gz), or a full run's Summary
  artifact directory (events.log + regression.csv). Only the tests filters keep are extracted.
  specs (log_parser.load_event_specs()) guides how raw logs are split into events."""
  if source.is_dir() and (source / 'events.log').is_file() and (source / 'regression.csv').is_file():
    return _collect_summary_logs(source, out_dirpath, filters)

  test_ids = []
  if source.is_dir():
    for filepath in sorted(source.rglob('*.tar.gz')):
      test_id = filepath.relative_to(source).as_posix()[:-len('.tar.gz')]
      if _is_filtered(test_id, filters) and ec._extract_test_tarball(filepath.read_bytes(), test_id, out_dirpath, specs):
        test_ids.append(test_id)
  else:
    with tarfile.open(source, mode='r|gz') as archive_strm:
      for member in archive_strm:
        if not member.isfile() or not member.name.endswith('.tar.gz'):
          continue
        parts = member.name[:-len('.tar.gz')].split('/', 1)
        if (len(parts) != 2) or not _is_filtered(parts[1], filters):
          continue
        if ec._extract_test_tarball(archive_strm.extractfile(member).read(), parts[1], out_dirpath, specs):
          test_ids.append(parts[1])
  return sorted(test_ids, key=str.lower)


def _check_ref(assertion) -> str:
  text = f'{Path(assertion.cpp_filepath).name}:{assertion.cpp_line}'
  if assertion.polarity == 'absent':
    text += ' (absent)'
  if assertion.skipped:
    text += ' (skipped)'
  return text


def _check_target(assertion) -> str:
  parts = [assertion.code]
  if assertion.symbol is not None:
    parts.append(f'"{assertion.symbol}"')
  if assertion.line > 0:
    parts.append(f'line {assertion.line}' + (f':{assertion.col}' if assertion.col > 0 else ''))
  return ' '.join(parts)


def _count_checks(assertions):
  """The cpp's expect-present checks that are not skipped, an any-of group counted once:
  (all of them, those whose event is in the log)."""
  units = defaultdict(list)
  for a in assertions:
    if a.polarity == 'present' and not a.skipped:
      units[('group', a.group) if a.group else ('check', a.aid)].append(a)
  shared = sum(1 for members in units.values() if any(a.matched for a in members))
  return len(units), shared


def _build_markdown(tests, reports_by_test, assertions_by_test):
  """One row per test that has a cpp or events of its own, plus a total."""
  rows = []
  totals = [0, 0, 0]
  for test_id in tests:
    in_cpp, shared = _count_checks(assertions_by_test.get(test_id, []))
    in_log = sum(1 for r in reports_by_test.get(test_id, []) if r.scope == 'local')
    if not tests[test_id].cpp_filepaths and not in_log:
      continue
    rows.append(f'| `{test_id}` | {in_cpp} | {in_log} | {shared} |')
    totals = [totals[0] + in_cpp, totals[1] + in_log, totals[2] + shared]
  out = ['## Event Check', '',
         '| Test | Events in cpp | Events in log | Shared |',
         '|---|---:|---:|---:|']
  out += rows
  out.append(f'| **Total** | **{totals[0]}** | **{totals[1]}** | **{totals[2]}** |')
  return '\n'.join(out)


def _write_csv(filepath, tests, reports_by_test, assertions_by_test, unparsed):
  with open(filepath, 'w', newline='', encoding='utf-8') as strm:
    writer = csv.writer(strm)
    writer.writerow(['Kind', 'Test', 'Cpp', 'Severity', 'Code', 'Number', 'Scope', 'Location', 'Symbol', 'Message',
                     'Status', 'Cpp check'])
    for test_id in tests:
      cpp = _cpp_names(tests[test_id]) or '-'
      for r in reports_by_test.get(test_id, []):
        writer.writerow(['event', test_id, cpp, r.severity, r.code, f'{r.prefix}{r.number}', r.scope, _location(r),
                         r.symbol or '', r.message, r.status, '; '.join(_check_ref(a) for a in r.matched_by)])
      for a in assertions_by_test.get(test_id, []):
        if a.status in _problem_statuses:
          writer.writerow(['check', test_id, cpp, '', a.code, '', '', '', a.symbol or '', _check_target(a), a.status,
                           _check_ref(a)])
    for u in unparsed:
      if u.test_id in tests:
        writer.writerow(['unparsed', u.test_id, _cpp_names(tests[u.test_id]) or '-', '', '', '', '', '', '', u.context, u.reason,
                         f'{Path(u.cpp_filepath).name}:{u.cpp_line}'])


def _main():
  parser = argparse.ArgumentParser(description='Match every event a PR\'s regression run logged against its test_*.cpp checks.')
  parser.add_argument('regression', nargs='+',
                      help='regression.py output directory (<test_id>.tar.gz per test), the '
                           'Regression_*_PR_<n>.tar.gz of a PR run\'s artifact, or the '
                           'Regression_*_<shard>.tar.gz of every shard of a full hlc run. Several are merged.')
  parser.add_argument('--filters', nargs='+', default=[], type=str,
                      help='Check only the tests whose whole id matches one of these (regression.py '
                           '--filters syntax, @file allowed).')
  parser.add_argument('--events-json', dest='events_json', nargs='+',
                      default=[str(_workspace_dirpath / 'build' / 'share' / library / 'events.json')
                               for library in ('hldb', 'hlc')],
                      help='The hlc install\'s event tables (share/hldb/events.json and share/hlc/events.json).')
  parser.add_argument('--test-dirpath', dest='test_dirpath', default=str(_workspace_dirpath / 'hlc'),
                      help='Directory holding the .hlc files and test_*.cpp files.')
  parser.add_argument('--output-dirpath', dest='output_dirpath', default=os.getcwd(),
                      help='Where event_check.md and event_check.csv are written (default: current directory).')
  parser.add_argument('--external-pattern', dest='external_pattern', default=ec._default_external_pattern,
                      help='Regex on a source path marking third-party library files.')
  parser.add_argument('--ignore-file', dest='ignore_file', default=str(_this_dirpath / 'ignored_events.txt'),
                      help='Event codes to leave out, one name per line. Pass an empty string to ignore nothing.')
  parser.add_argument('--jobs', type=int, default=os.cpu_count() or 1, help='Worker processes.')
  args = parser.parse_args()
  # The report contains emoji; do not depend on the console code page (cp1252 on Windows).
  sys.stdout.reconfigure(encoding='utf-8', errors='replace')

  test_dirpath = Path(args.test_dirpath).resolve()
  output_dirpath = Path(args.output_dirpath).resolve()
  output_dirpath.mkdir(parents=True, exist_ok=True)
  log_dirpath = output_dirpath / 'event_check_logs'
  log_dirpath.mkdir(parents=True, exist_ok=True)

  events_filepaths = [Path(filepath).resolve() for filepath in args.events_json]
  specs = log_parser.load_event_specs(events_filepaths)

  filters = build_filters(args.filters)
  run_test_ids = sorted({test_id for source in args.regression
                         for test_id in _collect_logs(Path(source).resolve(), log_dirpath, filters, specs)},
                        key=str.lower)
  print(f'Tests in the regression run: {len(run_test_ids)}', flush=True)

  codes = ec._load_code_table(events_filepaths)
  code_by_number = {c.number: c for c in codes.values()}
  all_code_names = set(codes)
  ignored = ec._load_ignore_list(Path(args.ignore_file)) if args.ignore_file else set()
  ignored &= all_code_names
  severities = ec._shown_severities(codes, ignored)

  all_tests = ec._scan_tests(test_dirpath, None)
  tests = {test_id: all_tests[test_id] for test_id in run_test_ids if test_id in all_tests}
  for test_id in run_test_ids:
    if test_id not in all_tests:
      print(f'Warning: {test_id} has no .hlc under {test_dirpath}; skipped', flush=True)
  links = ec._link_cpp(test_dirpath, tests)

  log_jobs = []
  for test in tests.values():
    log_filepath = ec._find_log(log_dirpath, test)
    if log_filepath is None:
      print(f'Warning: no log for {test.test_id}', flush=True)
    else:
      log_jobs.append((test, log_filepath, code_by_number, specs, args.external_pattern, _workspace_dirpath))
  cpp_jobs = [(targets, str(cpp), all_code_names) for targets, cpp in links]

  reports_by_test = {}
  assertions_by_test = defaultdict(list)
  unparsed = []
  with multiprocessing.Pool(max(1, min(args.jobs, len(log_jobs) + len(cpp_jobs)))) as pool:
    for test_id, reports, _ in pool.imap_unordered(ec._parse_log, log_jobs):
      # Notes, info and fully ignored severities (SNT) are not counted, as in the full run's summary.
      reports_by_test[test_id] = [r for r in reports if r.code not in ignored and r.severity in severities]
    aid = 0
    for targets, cpp, items, bad in pool.imap_unordered(ec._scan_cpp, cpp_jobs):
      unparsed.extend(bad)
      for item in ec._drop_ignored(items, ignored):
        for test_id in targets:
          aid += 1
          assertions_by_test[test_id].append(ec.Assertion(
            aid=aid, test_id=test_id, cpp_filepath=cpp, cpp_line=item['cpp_line'], test_name=item['test_name'],
            code=item['code'], symbol=item['symbol'], line=item['line'], col=item['col'],
            polarity=item['polarity'], skipped=item['skipped'], source=item['source'],
            group=item['group'], conditional=item['conditional']))
  ec._regroup(assertions_by_test)
  ec._join(tests, reports_by_test, assertions_by_test)

  markdown = _build_markdown(tests, reports_by_test, assertions_by_test)
  (output_dirpath / 'event_check.md').write_text(markdown + '\n', encoding='utf-8')
  _write_csv(output_dirpath / 'event_check.csv', tests, reports_by_test, assertions_by_test, unparsed)

  step_summary = os.environ.get('GITHUB_STEP_SUMMARY')
  if step_summary:
    with open(step_summary, 'a', encoding='utf-8') as strm:
      strm.write(markdown + '\n')

  print(markdown)
  print(f'Wrote {output_dirpath / "event_check.md"} and {output_dirpath / "event_check.csv"}')
  return 0


if __name__ == '__main__':
  sys.exit(_main())
