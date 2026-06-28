#!/usr/bin/env python3

"""
Batch wrapper around extract.py.
Accepts either a single zip archive or a directory of zip archives and
invokes extract.py for each one found.
"""

import argparse
import multiprocessing
import subprocess
import sys

from datetime import datetime
from pathlib import Path

_this_filepath = Path(__file__).resolve()
_extract_filepath = _this_filepath.parent / 'extract.py'


def _main():
  start_dt = datetime.now()
  print(f'Starting batch CI artifact extraction @ {str(start_dt)}')

  parser = argparse.ArgumentParser(
      description='Extract logs and/or DBs from one or more CI zip archives.')
  parser.add_argument('modes', nargs='+', choices=['db', 'log'], type=str, help='Pick what to extract')
  parser.add_argument(
      '--input', dest='input_path', required=True, type=str,
      help='Path to a single zip archive, or a directory containing zip archives.')
  parser.add_argument(
      '--output-dirpath', dest='output_dirpath', required=False, type=str,
      help='Output root directory. Each archive is extracted into a subdirectory named after the archive stem.')
  parser.add_argument(
      '--filters', nargs='+', required=False, default=[], type=str, help='Filter tests matching these regex inputs')
  parser.add_argument(
      '--jobs', nargs='?', required=False, default=multiprocessing.cpu_count(), type=int,
      help='Parallel jobs passed through to each extract.py invocation.')
  args = parser.parse_args()

  input_path = Path(args.input_path).resolve()

  if input_path.is_file():
    zip_files = [input_path]
  elif input_path.is_dir():
    zip_files = sorted(input_path.glob('Regression_*.zip'))
  else:
    print(f'Error: input path does not exist: {input_path}')
    return 1

  if not zip_files:
    print(f'No zip archives found at {input_path}')
    return 0

  print( 'Environment:')
  print(f'       command-line: {" ".join(sys.argv)}')
  print(f'         input-path: {input_path}')
  print(f'     output-dirpath: {args.output_dirpath}')
  print(f'            filters: {args.filters}')
  print(f'               jobs: {args.jobs}')
  print(f'archives to process: {len(zip_files)}')
  for zf in zip_files:
    print(f'    {zf}')
  print('')

  total_failures = 0
  for zip_filepath in zip_files:
    print(f'Processing {zip_filepath} ...')

    cmd = [sys.executable, str(_extract_filepath)] + args.modes
    cmd += ['--zip-filepath', str(zip_filepath)]

    if args.output_dirpath:
      archive_output = Path(args.output_dirpath) / zip_filepath.stem
      cmd += ['--output-dirpath', str(archive_output)]

    if args.filters:
      cmd += ['--filters'] + args.filters

    if args.jobs is not None:
      cmd += ['--jobs', str(args.jobs)]

    result = subprocess.run(cmd)
    total_failures += result.returncode
    print('')

  end_dt = datetime.now()
  delta = round((end_dt - start_dt).total_seconds())
  print(f'Batch extraction completed @ {str(end_dt)} in {str(delta)} seconds, with {total_failures} total failure(s).')
  return total_failures


if __name__ == '__main__':
  sys.exit(_main())
