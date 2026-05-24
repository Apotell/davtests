#!/usr/bin/env python

"""
Script to scan a directory looking for test logs and collect integration errors.
"""

import argparse
from collections import defaultdict
import multiprocessing
import os
import tarfile
import traceback
import zipfile
from pathlib import Path
from pathlibutil.json import (
  load as json_load,
  dump as json_dump,
  loads as json_loads
)

_platform_ids = ['.linux', '.osx', '.msys', '.win', '']
_max_input_file_count = 8
_reduction_start_marker = '= BEGIN REDUCTION RESULT ='
_reduction_end_marker = '= END REDUCTION RESULT ='


def _is_ci_build():
  return 'GITHUB_JOB' in os.environ


def _merge_dicts(dicta, dictb):
  return {
    key: dicta.get(key, 0) + dictb.get(key, 0)
    for key in set(dicta.keys()).union(set(dictb.keys()))
  }


def _scan_one(args):
  zip_filepath, shard = args
  archive_name = Path(zip_filepath).stem
  
  errors = []
  counts = defaultdict(int)
  categories = {}
  regression = []
  reducer = []
  env = {}
  with zipfile.ZipFile(zip_filepath, 'r') as zipfile_strm:
    with zipfile_strm.open(f'{archive_name}.tar.gz') as tarfile_strm:
      with tarfile.open(fileobj=tarfile_strm) as archive_strm:
        for test_archive_path in archive_strm.getnames():
          test_id = Path(test_archive_path).with_suffix('').with_suffix('').relative_to(archive_name).as_posix()
          test_archive_name = Path(test_id).name

          with tarfile.open(fileobj=archive_strm.extractfile(test_archive_path)) as test_archive_strm:
            for platform_id in _platform_ids:
              src_filepath = f'{test_archive_name}/{test_archive_name}{platform_id}.log'
      
              if src_filepath in test_archive_strm.getnames():
                try:
                  src_strm = test_archive_strm.extractfile(src_filepath)

                  reduction_scan_started = False
                  for line in src_strm: # pyright: ignore[reportOptionalIterable]
                    line = line.decode().rstrip()

                    if not line:
                      pass
                    elif not reduction_scan_started and _reduction_start_marker in line:
                      reduction_scan_started = True
                      continue
                    elif reduction_scan_started and _reduction_end_marker in line:
                      reduction_scan_started = False
                      continue
                    elif line.startswith(("[ERR:", "[WRN:", "[SNT:", "[FTL:")):
                      errors.append(f"{test_id}: {line}")
                      counts[test_id] += 1

                      category = line[:12]
                      categories[category] = categories.get(category, 0) + 1
                    elif line.startswith("Internal Error"):
                      errors.append(f"{test_id}: {line}")
                      category = "[  Internal]"
                      categories[category] = categories.get(category, 0) + 1

                    if reduction_scan_started:
                      parts = line.split()
                      if len(parts) == 4:
                        reducer.append(' | '.join(str(v) for v in [shard, test_id, *parts]))
      
                except Exception:
                  print(f"Failed to parse {src_filepath}")
                  traceback.print_last()

    with zipfile_strm.open(f'regression.csv') as regression_strm:
      lines = [_.decode('utf-8').strip() for _ in regression_strm]

      prefix = f'| {shard:>5} '
      regression.extend(prefix + _ for _ in lines[3:-1])

    with zipfile_strm.open(f'env.json') as env_strm:
      env = json_load(env_strm)

    return shard, errors, categories, counts, regression, reducer, env


def _write_errors_log(filepath, errors, counts, categories):
  with filepath.open("w") as strm:
    separator = '\n' + ('=' * 160) + '\n'

    strm.write("\n".join(sorted(errors)))
    strm.write(separator)

    max_name_len = max((len(name) for name in counts.keys()), default=40)
    strm.write("\n".join(f"{name:>{max_name_len}}: {counts[name]:>6}" for name in sorted(counts.keys())))
    strm.write(separator)

    strm.write("\n".join(f"{category}: {categories[category]:>6}" for category in sorted(categories.keys())))
    strm.write(f"\nFound {len(errors)} total errors.")
    strm.write(separator)

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
  args = parser.parse_args()
  
  input_dirpath = Path(args.input_dirpath)

  if (args.jobs == None) or (args.jobs > multiprocessing.cpu_count()):
    args.jobs = multiprocessing.cpu_count()

  filename_pattern = args.filename_pattern + '.zip'
  params = [
    (input_dirpath / filename_pattern.format(index=i), i) for i in range(_max_input_file_count)
    if (input_dirpath / filename_pattern.format(index=i)).exists()
  ]

  if not params:
    raise ValueError("Found no artifacts to scan!")

  errors = []
  categories = {}
  counts = defaultdict(int)
  regression = []
  reducer = []
  envs = json_loads(os.getenv('GITHUB_CONTEXT')) if _is_ci_build() else {} # pyright: ignore[reportArgumentType]
  envs['shards'] = {}

  if args.jobs <= 1:
    for filepath, shard in params:
      shard, errs, cats, cts, regs, reds, env = _scan_one((filepath, shard))
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

  _write_errors_log(input_dirpath / "errors.log", errors, counts, categories)
  _write_env_json(input_dirpath / "env.json", envs)
  _write_regression_csv(input_dirpath / "regression.csv", regression)
  _write_reducer_csv(input_dirpath / "reducer.csv", reducer)

  return 0


if __name__ == '__main__':
  import sys
  sys.exit(_main())
