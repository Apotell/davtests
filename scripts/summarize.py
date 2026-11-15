#!/usr/bin/env python

"""
Script to scan a directory looking for test logs and collect integration errors.
"""

import argparse
from collections import defaultdict
import io
import multiprocessing
import os
import tarfile
import traceback
import zipfile
from pathlib import Path
from pathlibutil.json import (
  load as json_load,
  dump as json_dump,
)

from ci_context import ci_context
from log_parser import LogEvent, iter_log, load_event_specs

_platform_ids = ['.linux', '.osx', '.msys', '.win', '']
_max_input_file_count = 8
_reduction_start_marker = '= BEGIN REDUCTION RESULT ='
_reduction_end_marker = '= END REDUCTION RESULT ='
_recorded_severities = ('ERR', 'WRN', 'SNT', 'FTL')


def _is_ci_build():
  return 'GITHUB_JOB' in os.environ


def _merge_dicts(dicta, dictb):
  return {
    key: dicta.get(key, 0) + dictb.get(key, 0)
    for key in set(dicta.keys()).union(set(dictb.keys()))
  }


def _scan_one(args):
  zip_filepath, shard, specs = args
  archive_name = Path(zip_filepath).stem
  
  errors = []
  counts = defaultdict(int)
  categories = {}
  regression = []
  reducer = []
  env = {}
  with zipfile.ZipFile(zip_filepath, 'r') as zipfile_strm:
    with zipfile_strm.open(f'{archive_name}.tar.gz') as tarfile_strm:
      # Stream mode ('r|*') reads the outer archive strictly sequentially. Random access
      # (getnames() + extractfile()) on a gzip stream nested inside a zip stream forces
      # a re-decompression from the start on every backward seek, i.e. O(N^2).
      with tarfile.open(fileobj=tarfile_strm, mode='r|*') as archive_strm:
        for test_archive_info in archive_strm:
          if not test_archive_info.isfile():
            continue

          test_archive_path = test_archive_info.name
          test_id = Path(test_archive_path).with_suffix('').with_suffix('').relative_to(archive_name).as_posix()
          test_archive_name = Path(test_id).name

          test_archive_data = archive_strm.extractfile(test_archive_info).read() # pyright: ignore[reportOptionalMemberAccess]
          with tarfile.open(fileobj=io.BytesIO(test_archive_data), mode='r:*') as test_archive_strm:
            test_archive_names = set(test_archive_strm.getnames())

            for platform_id in _platform_ids:
              src_filepath = f'{test_archive_name}/{test_archive_name}{platform_id}.log'

              if src_filepath in test_archive_names:
                try:
                  src_data = test_archive_strm.extractfile(src_filepath).read() # pyright: ignore[reportOptionalMemberAccess]

                  # log_parser splits the log into events (header plus continuation lines,
                  # '[debug]' lines dropped); every other line comes back as a str.
                  reduction_scan_started = False
                  for item in iter_log(src_data.decode().splitlines(), specs):
                    if isinstance(item, LogEvent):
                      if item.m_severity is None:
                        category = "[  Internal]"
                      elif item.m_severity in _recorded_severities:
                        category = item.header[:12]
                        counts[test_id] += 1
                      else:
                        continue

                      errors.append([f"{test_id}: {line}" for line in item.m_lines])
                      categories[category] = categories.get(category, 0) + 1
                    elif not reduction_scan_started and _reduction_start_marker in item:
                      reduction_scan_started = True
                    elif reduction_scan_started and _reduction_end_marker in item:
                      reduction_scan_started = False
                    elif reduction_scan_started:
                      parts = item.split()
                      if len(parts) == 4:
                        reducer.append(' | '.join(str(v) for v in [shard, test_id, *parts]))
      
                except Exception:
                  print(f"Failed to parse {src_filepath}")
                  traceback.print_exc()

    with zipfile_strm.open(f'regression.csv') as regression_strm:
      lines = [_.decode('utf-8').strip() for _ in regression_strm]

      prefix = f'| {shard:>5} '
      regression.extend(prefix + _ for _ in lines[3:-1])

    with zipfile_strm.open(f'env.json') as env_strm:
      env = json_load(env_strm)

    return shard, errors, categories, counts, regression, reducer, env


def _write_events_log(filepath, errors):
  """Writes every event, sorted by its header line, as its header followed by its continuation
  lines. Each line is prefixed with '<test_id>: '; continuation lines keep their indentation."""
  with filepath.open("w") as strm:
    for event in sorted(errors, key=lambda lines: lines[0]):
      for line in event:
        strm.write(line)
        strm.write('\n')

    strm.flush()


def _write_tests_csv(filepath, counts):
  with filepath.open("w") as strm:
    strm.write(' | '.join(['TESTNAME', 'COUNT']))
    strm.write('\n')

    for name in sorted(counts.keys()):
      strm.write(f'{name} | {counts[name]}\n')

    strm.flush()


def _write_event_ids_csv(filepath, categories):
  with filepath.open("w") as strm:
    strm.write(' | '.join(['EVENT_ID', 'COUNT']))
    strm.write('\n')

    for category in sorted(categories.keys()):
      strm.write(f'{category} | {categories[category]}\n')

    strm.flush()


def _write_events_json(filepath, events_filepaths):
  """Combines the event tables of the hlc install (share/hldb/events.json and
  share/hlc/events.json) into a single file. List-valued keys ('events', 'categories') are
  concatenated; any other key keeps the first file's value."""
  combined = {}
  for events_filepath in events_filepaths:
    with Path(events_filepath).open(encoding='utf-8') as strm:
      data = json_load(strm)

    for key, value in data.items():
      if isinstance(value, list):
        combined.setdefault(key, []).extend(value)
      else:
        combined.setdefault(key, value)

  with filepath.open("w", encoding='utf-8') as strm:
    json_dump(combined, strm, indent=2)
    strm.flush()


def _write_regression_csv(filepath, regression):
  with filepath.open("w") as strm:
    strm.write('| ' + ' | '.join([
      'SHARD', 'TESTNAME', 'STATUS', 'FATAL', 'SYNTAX', 'ERROR', 'WARNING',
      'NOTE', 'COVERAGE', 'REDUCTION', 'CPU-TIME', 'VTL-MEM', 'PHY-MEM'
    ]) + ' |\n')

    for line in regression:
      strm.write(line)
      strm.write('\n')

    strm.flush()

def _write_reducer_csv(filepath, reducer):
  with filepath.open("w") as strm:
    strm.write(' | '.join(['SHARD', 'TESTNAME', 'BEFORE', 'AFTER', 'ADDED', 'REMOVED']))
    strm.write('\n')

    for line in reducer:
      strm.write(line)
      strm.write('\n')

    strm.flush()


def _write_env_json(filepath, envs):
  with filepath.open("w") as strm:
    json_dump(envs, strm, indent=2)
    strm.flush()


def _main():
  parser = argparse.ArgumentParser()
  parser.add_argument('input_dirpath', type=str, help='Directory to scan')
  parser.add_argument('filename_pattern', type=str, help='Filename pattern')
  parser.add_argument(
      '--jobs', nargs='?', required=False, default=multiprocessing.cpu_count(), type=int,
      help='Run tests in parallel, optionally providing max number of concurrent processes. Set 0 to run sequentially.')
  parser.add_argument(
      '--events-json', dest='events_json', nargs='+', required=False, default=[], type=str,
      help='Event tables (share/hldb/events.json and share/hlc/events.json): they guide how the logs '
           'are split into events, and are combined into a single events.json.')
  args = parser.parse_args()

  input_dirpath = Path(args.input_dirpath)

  if (args.jobs == None) or (args.jobs > multiprocessing.cpu_count()):
    args.jobs = multiprocessing.cpu_count()

  # Without the event tables the logs are split on the event layout alone (see log_parser.py).
  specs = load_event_specs(args.events_json) if args.events_json else None

  filename_pattern = args.filename_pattern + '.zip'
  params = [
    (input_dirpath / filename_pattern.format(index=i), i, specs) for i in range(_max_input_file_count)
    if (input_dirpath / filename_pattern.format(index=i)).exists()
  ]

  if not params:
    raise ValueError("Found no artifacts to scan!")

  errors = []
  categories = {}
  counts = defaultdict(int)
  regression = []
  reducer = []
  envs = ci_context() if _is_ci_build() else {}
  envs['shards'] = {}

  if args.jobs <= 1:
    for param in params:
      shard, errs, cats, cts, regs, reds, env = _scan_one(param)
      errors.extend(errs)
      categories = _merge_dicts(categories, cats)
      counts.update(cts)
      regression.extend(regs)
      reducer.extend(reds)
      envs['shards'][f'env_{shard}'] = env
  else:
    max_processes = min(args.jobs, len(params))
    with multiprocessing.Pool(processes=max_processes) as pool:
      for shard, errs, cats, cts, regs, reds, env in pool.map(_scan_one, params):
        errors.extend(errs)
        categories = _merge_dicts(categories, cats)
        counts.update(cts)
        regression.extend(regs)
        reducer.extend(reds)
        envs['shards'][f'env_{shard}'] = env

  _write_events_log(input_dirpath / "events.log", errors)
  _write_tests_csv(input_dirpath / "tests.csv", counts)
  _write_event_ids_csv(input_dirpath / "event_ids.csv", categories)
  print(f"Found {len(errors)} total errors.")
  if args.events_json:
    _write_events_json(input_dirpath / "events.json", args.events_json)
  _write_env_json(input_dirpath / "env.json", envs)
  _write_regression_csv(input_dirpath / "regression.csv", regression)
  _write_reducer_csv(input_dirpath / "reducer.csv", reducer)

  return 0


if __name__ == '__main__':
  import sys
  sys.exit(_main())
