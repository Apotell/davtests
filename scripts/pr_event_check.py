#!/usr/bin/env python3

"""
Per-event check of a PR's regression run against the tests' test_*.cpp files.

For every test the PR's filtered regression ran, every event (FTL/SNT/ERR/WRN/NTE) in its log
is matched against the Test::findEvent() checks in the test's test_*.cpp, and the result is
reported one event per row. The checks a cpp makes that no event matches are reported too.

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

Writes event_check.md (also appended to the GitHub job summary) and event_check.csv.
The report never fails the job; ctest is what fails on a broken check.
"""

import argparse
import csv
import multiprocessing
import os
import re
import sys
import tarfile
from collections import Counter, defaultdict
from pathlib import Path

import event_coverage as ec

_this_dirpath = Path(__file__).parent.resolve()
_workspace_dirpath = _this_dirpath.parent

# Rows per test in the markdown; the CSV always has every row. GitHub caps a job summary at 1 MiB.
_max_md_rows_per_test = 100

_event_icons = {
  'Checked': '\u2705',
  'Not checked': '\U0001F7E1',
  'Contradiction': '\u274C',
  'Satisfied (skipped)': '\u23ED\uFE0F',
  'Known bogus': '\U0001F41E',
  'Conditional': '\u2754',
}

_problem_statuses = ('Broken', 'Pending', 'Satisfied (skipped)')
_problem_labels = {
  'Broken': '\u274C Expected by the cpp, not logged',
  'Pending': '\u23F8\uFE0F Skipped, still not holding',
  'Satisfied (skipped)': '\u23ED\uFE0F Skipped, but already holds',
}


def _collect_logs(source: Path, out_dirpath: Path) -> list[str]:
  """Extracts the logs of a regression run into <out>/<test_id>/<name>.log and returns the test
  ids. source is either regression.py's output directory (one <test_id>.tar.gz per test) or the
  PR artifact's tarball of that directory (<top>/<test_id>.tar.gz)."""
  test_ids = []
  if source.is_dir():
    for filepath in sorted(source.rglob('*.tar.gz')):
      test_id = filepath.relative_to(source).as_posix()[:-len('.tar.gz')]
      if ec._extract_test_tarball(filepath.read_bytes(), test_id, out_dirpath):
        test_ids.append(test_id)
  else:
    with tarfile.open(source, mode='r|gz') as archive_strm:
      for member in archive_strm:
        if not member.isfile() or not member.name.endswith('.tar.gz'):
          continue
        parts = member.name[:-len('.tar.gz')].split('/', 1)
        if len(parts) != 2:
          continue
        if ec._extract_test_tarball(archive_strm.extractfile(member).read(), parts[1], out_dirpath):
          test_ids.append(parts[1])
  return sorted(test_ids, key=str.lower)


def _short_path(path: str) -> str:
  return re.sub(r'^\$Source\d+/', '', path.replace('\\', '/'))


def _location(report) -> str:
  if not report.path:
    return ''
  text = _short_path(report.path)
  if report.line:
    text += f':{report.line}'
    if report.col:
      text += f':{report.col}'
  return text


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


def _md_escape(text: str, limit: int = 0) -> str:
  if limit and len(text) > limit:
    text = text[:limit - 3] + '...'
  return text.replace('|', '\\|').replace('\n', ' ')


def _test_icon(test, reports, problems) -> str:
  local = [r for r in reports if r.scope == 'local']
  if any(r.status == 'Contradiction' for r in reports) or any(a.status == 'Broken' for a in problems):
    return '\u274C'
  if not test.cpp_filepaths:
    return '\u26AA' if local else '\u2796'
  if not local:
    return '\u2796'
  checked = sum(1 for r in local if r.status == 'Checked')
  if checked == len(local):
    return '\u2705'
  return '\U0001F7E1' if checked else '\U0001F7E0'


def _build_markdown(tests, reports_by_test, assertions_by_test, unparsed, missing_logs, ignored):
  all_reports = [r for test_id in tests for r in reports_by_test.get(test_id, [])]
  local_reports = [r for r in all_reports if r.scope == 'local']
  problems_by_test = {
    test_id: [a for a in assertions_by_test.get(test_id, []) if a.status in _problem_statuses]
    for test_id in tests
  }
  all_problems = [a for ps in problems_by_test.values() for a in ps]

  out = ['## PR Event Check', '']
  out.append(f'Every event in the logs of the **{len(tests)}** test(s) this PR ran, matched against the '
             '`findEvent()` checks in their `test_*.cpp`.')
  if ignored:
    out.append(f'Ignored codes: {", ".join(f"`{name}`" for name in ignored)}.')
  out.append('')

  status_counts = Counter(r.status for r in local_reports)
  out += ['| Events from the tests\' own files | Count |', '|---|---:|']
  for status, icon in _event_icons.items():
    if status_counts.get(status):
      out.append(f'| {icon} {status} | {status_counts[status]} |')
  out.append(f'| **Total** | **{len(local_reports)}** |')
  external = len(all_reports) - len(local_reports)
  if external:
    out.append(f'| Library events (UVM/OVM files), not listed below | {external} |')
  out.append('')

  if all_problems:
    problem_counts = Counter(a.status for a in all_problems)
    out += ['| cpp checks no event satisfies | Count |', '|---|---:|']
    for status in _problem_statuses:
      if problem_counts.get(status):
        out.append(f'| {_problem_labels[status]} | {problem_counts[status]} |')
    out.append('')

  if missing_logs:
    out.append(f'**No log** for: {", ".join(f"`{t}`" for t in sorted(missing_logs))}')
    out.append('')
  if unparsed:
    out.append(f'**{len(unparsed)}** `findEvent()` call(s) could not be read statically; see event_check.csv.')
    out.append('')

  # A test with no cpp and no events of its own has nothing to cross-check; list them in one line.
  quiet = [test_id for test_id in tests
           if not tests[test_id].cpp_filepaths and test_id not in missing_logs
           and not any(r.scope == 'local' for r in reports_by_test.get(test_id, []))]

  out += ['### Per test', '']
  if quiet:
    out.append(f'\u2796 No cpp and no events of their own ({len(quiet)}): '
               + ', '.join(f'`{t}`' for t in quiet))
    out.append('')
  for test_id in tests:
    if test_id in quiet:
      continue
    test = tests[test_id]
    reports = reports_by_test.get(test_id, [])
    local = [r for r in reports if r.scope == 'local']
    problems = problems_by_test[test_id]
    checked = sum(1 for r in local if r.status == 'Checked')
    cpp_text = ', '.join(f'`{p.name}`' for p in test.cpp_filepaths) if test.cpp_filepaths else 'no test_*.cpp'
    summary = f'{_test_icon(test, reports, problems)} <code>{test_id}</code> -- {checked}/{len(local)} checked'
    if problems:
      summary += f', {len(problems)} check problem(s)'
    out.append(f'<details><summary>{summary}</summary>')
    out.append('')
    out.append(f'cpp: {cpp_text}')
    out.append('')
    if local:
      out += ['| | Code | Location | Symbol | Status | Checked by |', '|---|---|---|---|---|---|']
      for r in local[:_max_md_rows_per_test]:
        checked_by = ', '.join(_check_ref(a) for a in r.matched_by)
        out.append(f'| {r.severity} | `{r.code}` [{r.prefix}{r.number}] | {_md_escape(_location(r))} | '
                   f'{_md_escape(r.symbol or "", 60)} | {_event_icons.get(r.status, "")} {r.status} | '
                   f'{_md_escape(checked_by)} |')
      if len(local) > _max_md_rows_per_test:
        out.append(f'| | ... {len(local) - _max_md_rows_per_test} more in event_check.csv | | | | |')
      out.append('')
    else:
      out += ['No events from the test\'s own files.', '']
    if problems:
      out += ['| Check | Expects | Status |', '|---|---|---|']
      for a in problems:
        out.append(f'| {_md_escape(_check_ref(a))} | {_md_escape(_check_target(a))} | {_problem_labels[a.status]} |')
      out.append('')
    out += ['</details>', '']
  return '\n'.join(out)


def _write_csv(filepath, tests, reports_by_test, assertions_by_test, unparsed):
  with open(filepath, 'w', newline='', encoding='utf-8') as strm:
    writer = csv.writer(strm)
    writer.writerow(['Kind', 'Test', 'Severity', 'Code', 'Number', 'Scope', 'Location', 'Symbol', 'Message',
                     'Status', 'Cpp check'])
    for test_id in tests:
      for r in reports_by_test.get(test_id, []):
        writer.writerow(['event', test_id, r.severity, r.code, f'{r.prefix}{r.number}', r.scope, _location(r),
                         r.symbol or '', r.message, r.status, '; '.join(_check_ref(a) for a in r.matched_by)])
      for a in assertions_by_test.get(test_id, []):
        if a.status in _problem_statuses:
          writer.writerow(['check', test_id, '', a.code, '', '', '', a.symbol or '', _check_target(a), a.status,
                           _check_ref(a)])
    for u in unparsed:
      if u.test_id in tests:
        writer.writerow(['unparsed', u.test_id, '', '', '', '', '', '', u.context, u.reason,
                         f'{Path(u.cpp_filepath).name}:{u.cpp_line}'])


def _main():
  parser = argparse.ArgumentParser(description='Match every event a PR\'s regression run logged against its test_*.cpp checks.')
  parser.add_argument('regression', help='regression.py output directory (<test_id>.tar.gz per test), or the '
                                         'Regression_*_PR_<n>.tar.gz of a PR run\'s artifact.')
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

  run_test_ids = _collect_logs(Path(args.regression).resolve(), log_dirpath)
  print(f'Tests in the regression run: {len(run_test_ids)}', flush=True)

  codes = ec._load_code_table([Path(filepath).resolve() for filepath in args.events_json])
  code_by_number = {c.number: c for c in codes.values()}
  all_code_names = set(codes)
  ignored = ec._load_ignore_list(Path(args.ignore_file)) if args.ignore_file else set()
  ignored &= all_code_names

  all_tests = ec._scan_tests(test_dirpath, None)
  tests = {test_id: all_tests[test_id] for test_id in run_test_ids if test_id in all_tests}
  for test_id in run_test_ids:
    if test_id not in all_tests:
      print(f'Warning: {test_id} has no .hlc under {test_dirpath}; skipped', flush=True)
  links = ec._link_cpp(test_dirpath, tests)

  missing_logs = set()
  log_jobs = []
  for test in tests.values():
    log_filepath = ec._find_log(log_dirpath, test)
    if log_filepath is None:
      missing_logs.add(test.test_id)
    else:
      log_jobs.append((test, log_filepath, code_by_number, args.external_pattern, _workspace_dirpath))
  cpp_jobs = [(targets, str(cpp), all_code_names) for targets, cpp in links]

  reports_by_test = {}
  assertions_by_test = defaultdict(list)
  unparsed = []
  with multiprocessing.Pool(max(1, min(args.jobs, len(log_jobs) + len(cpp_jobs)))) as pool:
    for test_id, reports, _ in pool.imap_unordered(ec._parse_log, log_jobs):
      reports_by_test[test_id] = [r for r in reports if r.code not in ignored]
    aid = 0
    for targets, cpp, items, bad in pool.imap_unordered(ec._scan_cpp, cpp_jobs):
      unparsed.extend(bad)
      for item in items:
        if item['code'] in ignored:
          continue
        for test_id in targets:
          aid += 1
          assertions_by_test[test_id].append(ec.Assertion(
            aid=aid, test_id=test_id, cpp_filepath=cpp, cpp_line=item['cpp_line'], test_name=item['test_name'],
            code=item['code'], symbol=item['symbol'], line=item['line'], col=item['col'],
            polarity=item['polarity'], skipped=item['skipped'], source=item['source'],
            group=item['group'], conditional=item['conditional']))
  ec._regroup(assertions_by_test)
  ec._join(tests, reports_by_test, assertions_by_test)

  markdown = _build_markdown(tests, reports_by_test, assertions_by_test, unparsed, missing_logs, sorted(ignored))
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
