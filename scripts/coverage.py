#!/usr/bin/env python3

import argparse
import functools
import multiprocessing
import pprint
import pyhldb # pyright: ignore[reportMissingImports]
import re
import sys
import tabulate
import traceback

from contextlib import redirect_stdout, redirect_stderr
from datetime import datetime
from enum import Enum, unique
from pathlib import Path
from pathlibutil.json import load as json_load
from typing import Dict, List, Set
from utils import build_filters, log


_this_filepath = Path(__file__).resolve()


def _scan(dirpath, filters):
  def _is_filtered(test_id, name):
    if not filters:
      return True
    for filter in filters:
      if isinstance(filter, str):
        if filter.lower() in (test_id.lower(), name.lower()):
          return True
      else:
        if filter.search(test_id) or filter.search(name):  # Note: match() reports success only if the match is at index 0
          return True
    return False

  all_tests = {}
  filtered_tests = set()
  for regression_log in dirpath.rglob('regression.log'):
    test_dirpath = regression_log.parent
    name = test_dirpath.name
    test_id = test_dirpath.relative_to(dirpath).as_posix()
    all_tests[test_id] = test_dirpath

    if _is_filtered(test_id, name):
      filtered_tests.add(test_id)

  return [
    { test_id: all_tests[test_id] for test_id in sorted(all_tests.keys(), key=lambda t: t.lower()) },
    { test_id: all_tests[test_id] for test_id in sorted(filtered_tests, key=lambda t: t.lower()) },
  ]


def _mounted(p: str, mounts: dict[str, str]) -> str:
    for vn, mp in mounts.items():
      p = p.replace(vn, mp)
    return p


class Visitor(pyhldb.Visitor):
  def __init__(self, file_contents: Dict[str, list[str]]):
    super().__init__()
    self.file_contents = file_contents
    self.ignored_types = [
      pyhldb.AnyType.Comment,
      pyhldb.AnyType.PreprocMacroDefinition,
      pyhldb.AnyType.PreprocMacroInstance,
      pyhldb.AnyType.SourceFile
    ]

  def visitAny(self, obj):
    if (obj.pp_start_line == 0) or (obj.pp_end_line == 0) or (obj.pp_start_column == 0) or (obj.pp_end_column == 0):
      return

    if obj.pp_start_line != obj.pp_end_line:
      return

    if obj.any_type in self.ignored_types:
      return

    logical = obj.pp_file
    if logical not in self.file_contents:
      return

    line = self.file_contents[logical][obj.pp_start_line - 1]
    line = line[:obj.pp_start_column - 1] + (' ' * (obj.pp_end_column - obj.pp_start_column)) + line[obj.pp_end_column - 1:]
    self.file_contents[logical][obj.pp_start_line - 1] = line


@functools.cache
def load_reserved_keywords():
  filepath = _this_filepath.parent / 'reserved_keywords.txt'

  words = set()
  with filepath.open() as strm:
    for word in strm:
      words.add(word.strip())

  return words


def _remove_comments(lines: List[str]) -> List[str]:
  # Ref: https://www.w3reference.com/blog/remove-c-and-c-comments-using-python/
  @unique
  class State(Enum):
    NORMAL = 0
    IN_STRING = 1
    IN_CHAR = 2
    IN_LINE_COMMENT = 3
    IN_BLOCK_COMMENT = 4

  current_state = State.NORMAL
  escaped = False  # Tracks escaped characters in strings/chars
  prev_char = None  # Previous character (for detecting // or /*)

  for i, line in enumerate(lines):
    output = []
    for char in line:
      if current_state == State.NORMAL:
        if prev_char == '/' and char == '/':
          # Enter line comment: remove the previous '/' (added by mistake)
          output.pop()
          current_state = State.IN_LINE_COMMENT
        elif prev_char == '/' and char == '*':
          # Enter block comment: remove the previous '/'
          output.pop()
          output.append('  ')
          current_state = State.IN_BLOCK_COMMENT
        elif char == '"':
          current_state = State.IN_STRING
          output.append(char)
        # elif char == "'":
        #   current_state = State.IN_CHAR
        #   output.append(char)
        else:
          output.append(char)
 
      elif current_state == State.IN_STRING:
        output.append(char)
        if char == '"' and not escaped:
          current_state = State.NORMAL
        escaped = (char == '\\') and not escaped  # Handle escapes: \"
 
      # elif current_state == State.IN_CHAR:
      #   output.append(char)
      #   if char == "'" and not escaped:
      #     current_state = State.NORMAL
      #   escaped = (char == '\\') and not escaped  # Handle escapes: \'
 
      elif current_state == State.IN_LINE_COMMENT:
        if char == '\n':
          # End line comment, preserve newline
          output.append(char)
          current_state = State.NORMAL
 
      elif current_state == State.IN_BLOCK_COMMENT:
        output.append(' ')
        if prev_char == '*' and char == '/':
          # End block comment: do not add '*' or '/' to output
          current_state = State.NORMAL
 
      # Update previous character (unless in block comment, where we skip adding chars)
      prev_char = char

    lines[i] = ''.join(output).rstrip()

  return lines


def _load_file_content(filepath: Path):
  with filepath.open('rt') as strm:
    lines = strm.readlines()

  return _remove_comments(lines)


def _save_file_content(filepath: Path, content: list[str]):
  with filepath.open('wt') as strm:
    for line in content:
      strm.write(line.strip())
      strm.write('\n')
    strm.flush()


def _compute_length(content: List[str]) -> int:
  return sum(len(line.strip()) for line in content)


def _mask_reserved_keywords(content: List[str], keywords: Set[str]):
  re_word = re.compile(r'\b\w+\b')
  re_spaces = re.compile(r'\s+')
  re_alpha = re.compile(r'[,:;&=@#)}\]\?\(\{\[\+\.\/\|\-\<\>\*\^]')

  for i, line in enumerate(content):
    for match in re_word.finditer(line):
      if match.group(0) in keywords:
        line = line[:match.start()] + (' ' * (match.end() - match.start())) + line[match.end():]

    line = re_alpha.sub(' ', line)
    line = re_spaces.sub(' ', line)
    content[i] = line

  return content


def _run_one(args):
  test_dirpath, keywords, test_id = args

  log(f'Running {test_id} ...')

  test_name = str(test_id)
  hldb_filepath = test_dirpath / 'design.hldb'
  coverage_log_filepath = test_dirpath / 'coverage.log'
  mounts_filepath = test_dirpath / 'mounts.json'

  result = {
    'test_name': test_name,
    'files': {},
    'file_count': 0,
    'pre_len': 0,
    'post_len': 0,
    'error_count': 0,
    'coverage': 0,
  }
  with coverage_log_filepath.open('w') as coverage_log_strm, \
          redirect_stdout(coverage_log_strm), \
          redirect_stderr(coverage_log_strm):
    try:
      print( 'Environment:')
      print(f'      test-name: {test_name}')
      print(f'   test-dirpath: {test_dirpath.as_posix()}')
      print(f'  hldb-filepath: {hldb_filepath.as_posix()}')
      print(f'mounts-filepath: {mounts_filepath.as_posix()}')
      print()

      # Load mounted paths to resolve logical paths in binary
      mounts = json_load(mounts_filepath.open())

      if hldb_filepath.exists():
        s = pyhldb.Serializer()
        designs = s.restore(str(hldb_filepath))

        file_contents = {}
        logical_to_path = {}
        for design in designs:
          for sf in design.source_files:
            filepath = Path(_mounted(sf.preproc_file, mounts))

            if filepath.is_file():
              # NOTE(HS): When loaded from cache, the pp file
              # doesn't exist at this location.
              content = _load_file_content(filepath)
              file_contents[sf.preproc_file] = content
              logical_to_path[sf.preproc_file] = filepath

              pre_len = _compute_length(content)
              _save_file_content(filepath.with_stem(f'{filepath.stem}_pre') , content)

              result['files'][sf.preproc_file] = {
                'pre_len': pre_len,
                'post_len': 0,
              }

        result['file_count'] = len(file_contents)

        if file_contents:
          visitor = Visitor(file_contents)
          for design in designs:
            visitor.visit(design)

          for logical, content in file_contents.items():
            content = _mask_reserved_keywords(content, keywords)

            result['files'][logical]['post_len'] = _compute_length(content)
            filepath = logical_to_path[logical]
            _save_file_content(filepath.with_stem(f'{filepath.stem}_post'), content)
        else:
          print(f'FAILED to find any source files.')
          result['error_count'] += 1
      else:
        print(f'FAILED to find hldb database!')
        result['error_count'] += 1

    except:
      print(f'{test_name} FAILED with exception!')
      result['error_count'] += 1
      traceback.print_exc()

    for _, r in result['files'].items():
      result['pre_len'] += r['pre_len']
      result['post_len'] += r['post_len']
    result['file_count'] = len(result['files'])
    result['coverage'] = (
      round(((result['pre_len'] - result['post_len']) / result['pre_len']) * 100, 2)
      if result['pre_len'] > 0 else 0
    )

    print()
    pprint.pprint({'result': result})
    _print_result(result)

    coverage_log_strm.flush()

  log(f'... {test_id} Completed.')
  return result


def _print_result(result):
  headers = ['INPUT', 'PRELEN', 'POSTLEN']
  rows = [(
    key,
    value['pre_len'],
    value['post_len']
  ) for key, value in result['files'].items()]
  rows.append(tabulate.SEPARATING_LINE) # pyright: ignore[reportArgumentType]
  rows.append(('', result['pre_len'], result['post_len']))

  print()
  print(tabulate.tabulate(rows, headers=headers, tablefmt="outline"))
  print()
  print(f'[   ERROR] : {result["error_count"]}')
  print(f'[COVERAGE] : {result["coverage"]}')
  print()


def _print_report(args, results):
  keys = [ 'test_name', 'error_count', 'file_count', 'pre_len', 'post_len', 'coverage' ]
  columns = [ 'TESTNAME', 'ERRORS', 'FILES', 'PRELEN', 'POSTLEN', 'COVERAGE' ]
  results = sorted(results, key=lambda r: r['test_name'])
  rows = [ [ result[keys[i]] for i in range(0, len(keys)) ] for result in results ]

  print('Results:')
  print(tabulate.tabulate(rows, headers=columns, tablefmt="outline", floatfmt=".2f"))

  filepath = args.base_dirpath / 'coverage.csv'
  with filepath.open('w') as strm:
    strm.write(tabulate.tabulate(rows, headers=columns, tablefmt="outline", floatfmt=".2f"))
    strm.flush()


def _run(args, tests):
  keywords = load_reserved_keywords()
  params = [(dirpath, keywords, test_id) for test_id, dirpath in tests.items()]

  if args.jobs <= 1:
    results = [_run_one(param) for param in params]
  else:
    with multiprocessing.Pool(processes=args.jobs) as pool:
      results = pool.map(_run_one, params)

  print('')
  _print_report(args, results)

  return sum([entry['error_count'] for entry in results])


def _main():
  start_dt = datetime.now()
  print(f'Starting Coverage Regression @ {str(start_dt)}')

  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--base-dirpath', dest='base_dirpath', required=True, type=str,
      help='Base directory path, either absolute or relative to current directory, to scan for tests.')
  parser.add_argument(
      '--filters', nargs='+', required=False, default=[], type=str, help='Filter tests matching these regex inputs')
  parser.add_argument(
      '--jobs', nargs='?', required=False, default=multiprocessing.cpu_count(), type=int,
      help='Run in parallel, optionally providing max number of concurrent processes. Set 0 to run sequentially.')
  parser.add_argument('--verbose', required=False, default=False, action='store_true', help='Generate verbose logs.')
  args = parser.parse_args()

  args.base_dirpath = Path(args.base_dirpath).resolve()
  args.filters = build_filters(args.filters)
  all_tests, filtered_tests = _scan(args.base_dirpath, args.filters)

  if (args.jobs == None) or (args.jobs > multiprocessing.cpu_count()):
    args.jobs = multiprocessing.cpu_count()

  if args.jobs > len(filtered_tests):
    args.jobs = len(filtered_tests)

  print( 'Environment:')
  print(f'    command-line: {" ".join(sys.argv)}')
  print(f' current-dirpath: {Path.cwd()}')
  print(f'    base-dirpath: {args.base_dirpath}')
  print(f'        max-jobs: {args.jobs}')
  print(f'       max-tests: {len(all_tests)}')
  print(f'  filtered-tests: {len(filtered_tests)}')
  print( '\n')

  print(f'Running {len(filtered_tests)} tests ...')
  result = _run(args, filtered_tests)
  print('\n')

  end_dt = datetime.now()
  delta = round((end_dt - start_dt).total_seconds())
  print(f'Coverage Regression Completed @ {str(end_dt)} in {str(delta)} seconds')
  return result

if __name__ == '__main__':
  sys.exit(_main())
