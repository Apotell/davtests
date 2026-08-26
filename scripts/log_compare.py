#!/usr/bin/env python3

"""
Tolerant comparison for hlc regression log files -- decides whether a freshly generated log is
MEANINGFULLY different from one already on disk (e.g. one already committed), not whether it is
byte-identical. Used by extract.py/extract_all.py to avoid overwriting a committed log with one
that only differs in ways that are expected to vary from run to run, which otherwise creates
unnecessary revision history.

Three, and only three, kinds of run-to-run noise are tolerated:

  1. The "PROFILE" ... "Wall-clock total: N ms" block -- wall-clock timings and cache stats that
     are never expected to be reproducible. Its content is never inspected at all, only located
     and discarded from both sides before anything else runs.
  2. "AST_DEBUG_BEGIN" ... "AST_DEBUG_END" blocks (FileContent::printTree()'s debug dump) -- one
     per file processed, emitted in whatever order that file happened to finish in (not
     deterministic under -mt). Matched between old and new by identity (their own "FILE:" header,
     the real path -- never by position), and even then only after normalizing every internal
     "f<N>" file-id reference to the real path it resolves to via that same block's own
     FILE:/INCL header lines: the numeric id is a per-run path-registration-order artifact, not a
     structural property of the AST, and can legitimately differ between two runs even for what
     is otherwise the exact same tree.
  3. "[TAG:CODE] ..." diagnostic messages (info/warning/error/etc., plus their continuation
     lines -- see _is_continuation_line() for the two shapes that come in) -- can legitimately
     emit in a different relative order under -mt. Matched between old and new as a multiset of
     exact text, never by position.

Everything left over after excising all of the above from both sides -- banners, the
FATAL/SYNTAX/ERROR/WARNING/NOTE summary counts, an HLDB object dump, or anything else -- must
match exactly, in order. This is the safety net: nothing outside the three categories above is
ever silently ignored, so a real regression there is always caught.

This is a narrower, purpose-built tool than scripts/compare_pp_logs.py (which compares hlc_01 vs.
hlc_02 output across an architectural rewrite, and so also tolerates cross-pipeline differences
like a missing diagnostic column). Both sides compared here come from the same tool, so no such
cross-pipeline tolerance is needed -- only reordering/renumbering noise. Some patterns (grouping
messages into a multiset keyed by their own text, extracting a delimited block by start/end
marker lines) are adapted from that script.
"""

from __future__ import annotations

import argparse
import difflib
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

PROFILE_START_RE = re.compile(r'^PROFILE\s*$')
PROFILE_END_RE = re.compile(r'^Wall-clock total:')

AST_START_RE = re.compile(r'^AST_DEBUG_BEGIN\s*$')
AST_END_RE = re.compile(r'^AST_DEBUG_END\s*$')
# "FILE:" (FileContent::printTree, the common case) and "FILE " (printObjects' empty-tree
# fallback) are both emitted by the C++ side -- tolerate either spelling here rather than assume
# one is a bug worth depending on.
_AST_FILE_RE = re.compile(r'^FILE:?\s+f<(\d+)>:\s*(.*)$')
_AST_INCL_RE = re.compile(r'^INCL\s+f<(\d+)>:\s*(.*)$')
_AST_LIB_RE = re.compile(r'^LIB:\s*(.*)$')
_AST_FILEID_TOKEN_RE = re.compile(r'f<(\d+)>')

# "[ERR:PP0109] rest of first line" -- a genuine diagnostic. Deliberately does NOT match a summary
# line like "[  FATAL] : 1" (no colon between the tag and what follows the closing bracket), so
# those counts fall through to the strict "everything else" comparison instead of being treated as
# a reorderable message.
MESSAGE_START_RE = re.compile(r'^\[([A-Z]+):(\S+?)\]\s?')

# AntlrParserErrorListener::syntaxError() (ParseFile.cpp) builds a [SNT:...] syntax error's own
# extra "location" as: the raw, verbatim echoed source line (no marker of its own -- its leading
# whitespace, if any, is simply whatever the original source line happened to have, so it can't be
# relied on either way), followed by a SEPARATE "^-- file:line:col:" pointer line. Only the pointer
# line has a fixed, recognizable prefix; the echoed source line does not, so it's pulled in via
# lookahead -- "is the line right after this one the pointer line" -- rather than by its own shape.
_CARET_POINTER_RE = re.compile(r'^\^-- ')


@dataclass
class Difference:
  kind: str
  detail: str


def _find_blocks(lines: list[str], start_re: re.Pattern, end_re: re.Pattern) -> tuple[list[tuple[int, int]], list[int]]:
  """Scans lines for every start_re..end_re span (inclusive, non-nesting). Returns (blocks,
  unterminated): `blocks` is a list of (start_idx, end_idx) index pairs; `unterminated` is a list
  of start indices whose start_re never found a matching end_re before EOF -- callers must not
  guess where those end (see this module's own "never silently ignore" rule) or exclude them from
  anything; they are simply left for the caller's normal comparison to catch.
  """
  blocks: list[tuple[int, int]] = []
  unterminated: list[int] = []
  i, n = 0, len(lines)
  while i < n:
    if start_re.match(lines[i]):
      start = i
      j = i + 1
      while j < n and not end_re.match(lines[j]):
        j += 1
      if j < n:
        blocks.append((start, j))
        i = j + 1
      else:
        unterminated.append(start)
        i = n
    else:
      i += 1
  return blocks, unterminated


def _excise_ranges(lines: list[str], ranges: list[tuple[int, int]]) -> list[str]:
  """Returns lines with every (start, end) inclusive index range removed. Ranges may be given in
  any order and must not overlap (true for every pair of categories this module excises -- their
  start-line patterns are mutually exclusive by construction)."""
  result: list[str] = []
  i = 0
  for start, end in sorted(ranges):
    result.extend(lines[i:start])
    i = end + 1
  result.extend(lines[i:])
  return result


def _ast_block_identity(block_lines: list[str]) -> tuple[str | None, str] | None:
  """Returns (lib, file_path) -- the stable identity to match an AST_DEBUG block by across two
  runs -- read from the block's own "LIB:"/"FILE:" header lines. Returns None if no "FILE:"
  header could be found at all (an unrecognized/malformed block -- the caller treats this as
  never matching anything, rather than guessing an identity for it)."""
  lib = None
  file_path = None
  for line in block_lines:
    m = _AST_LIB_RE.match(line)
    if m:
      lib = m.group(1)
      continue
    m = _AST_FILE_RE.match(line)
    if m:
      file_path = m.group(2)
  return None if file_path is None else (lib, file_path)


def _normalize_ast_block(block_lines: list[str]) -> list[str]:
  """Replaces every "f<N>" file-id token in block_lines with "f<REALPATH>", using this same
  block's own FILE:/INCL header lines as the id -> real-path table -- see this module's own top
  comment for why the raw numeric id is never comparable across two runs. A token whose id isn't
  in this block's own table (should not happen for a well-formed dump, but not assumed) is left
  as-is rather than guessed at.
  """
  id_to_path: dict[str, str] = {}
  for line in block_lines:
    m = _AST_FILE_RE.match(line)
    if m:
      id_to_path[m.group(1)] = m.group(2)
      continue
    m = _AST_INCL_RE.match(line)
    if m:
      id_to_path[m.group(1)] = m.group(2)

  def normalize_line(line: str) -> str:
    def repl(m: re.Match) -> str:
      real = id_to_path.get(m.group(1))
      return f'f<{real}>' if real is not None else m.group(0)
    return _AST_FILEID_TOKEN_RE.sub(repl, line)

  return [normalize_line(line) for line in block_lines]


def _compare_ast_blocks(old_lines: list[str], new_lines: list[str]) -> list[Difference]:
  old_blocks, old_unterminated = _find_blocks(old_lines, AST_START_RE, AST_END_RE)
  new_blocks, new_unterminated = _find_blocks(new_lines, AST_START_RE, AST_END_RE)

  diffs: list[Difference] = []
  for start in old_unterminated:
    diffs.append(Difference('ast-block-unterminated', f'old:{start + 1}: AST_DEBUG_BEGIN with no matching AST_DEBUG_END'))
  for start in new_unterminated:
    diffs.append(Difference('ast-block-unterminated', f'new:{start + 1}: AST_DEBUG_BEGIN with no matching AST_DEBUG_END'))

  def group_by_identity(lines: list[str], blocks: list[tuple[int, int]]):
    grouped: dict[tuple[str | None, str] | None, list[list[str]]] = {}
    for s, e in blocks:
      body = lines[s:e + 1]
      key = _ast_block_identity(body)
      grouped.setdefault(key, []).append(_normalize_ast_block(body) if key is not None else body)
    return grouped

  old_by_key = group_by_identity(old_lines, old_blocks)
  new_by_key = group_by_identity(new_lines, new_blocks)

  # dict.fromkeys() preserves first-seen order for stable, readable output.
  for key in dict.fromkeys(list(old_by_key) + list(new_by_key)):
    old_list = old_by_key.get(key, [])
    new_list = new_by_key.get(key, [])
    label = f'FILE:{key[1]} (LIB:{key[0]})' if key is not None else '<block with no recognizable FILE: header>'

    if key is None:
      # Can't match these individually -- fall back to requiring the same multiset of raw bodies.
      if Counter(map(tuple, old_list)) != Counter(map(tuple, new_list)):
        diffs.append(Difference('ast-block-differs', f'{label}: content differs'))
      continue

    paired = min(len(old_list), len(new_list))
    for old_body, new_body in zip(old_list[:paired], new_list[:paired]):
      if old_body != new_body:
        diffs.append(Difference('ast-block-differs', f'{label}: content differs (after f<N> normalization)'))
    for _ in old_list[paired:]:
      diffs.append(Difference('ast-block-missing-in-new', label))
    for _ in new_list[paired:]:
      diffs.append(Difference('ast-block-extra-in-new', label))

  return diffs


def _is_continuation_line(lines: list[str], i: int) -> bool:
  """A continuation line is either (a) the usual indented "file:line:col: ..." style reference, or
  (b) a [SNT:...] syntax error's own echoed-source-line + "^-- file:line:col:" pointer pair (see
  this module's own _CARET_POINTER_RE comment) -- recognized by the POINTER line's fixed "^-- "
  prefix, with the (otherwise unmarked, arbitrarily-indented-or-not) line right before it pulled in
  via lookahead rather than by its own shape."""
  line = lines[i]
  if not line or MESSAGE_START_RE.match(line):
    return False
  if line[0].isspace() or _CARET_POINTER_RE.match(line):
    return True
  return (i + 1 < len(lines)) and bool(_CARET_POINTER_RE.match(lines[i + 1]))


def _message_blocks(lines: list[str]) -> list[tuple[int, int, str]]:
  """Returns (start_idx, end_idx, text) for every "[TAG:CODE] ..." message plus its continuation
  lines -- see _is_continuation_line() for exactly what counts as one."""
  blocks: list[tuple[int, int, str]] = []
  i, n = 0, len(lines)
  while i < n:
    if MESSAGE_START_RE.match(lines[i]):
      start = i
      i += 1
      while i < n and _is_continuation_line(lines, i):
        i += 1
      blocks.append((start, i - 1, '\n'.join(lines[start:i])))
    else:
      i += 1
  return blocks


def _compare_messages(old_lines: list[str], new_lines: list[str]) -> list[Difference]:
  """Compares messages as a multiset of exact text -- order is never significant (see this
  module's own top comment); a message present the same number of times on both sides, anywhere,
  is not a difference."""
  old_counts = Counter(text for _, _, text in _message_blocks(old_lines))
  new_counts = Counter(text for _, _, text in _message_blocks(new_lines))

  diffs: list[Difference] = []
  for text, count in (old_counts - new_counts).items():
    diffs.append(Difference('message-missing-in-new', f'{count}x: {text.splitlines()[0]}'))
  for text, count in (new_counts - old_counts).items():
    diffs.append(Difference('message-extra-in-new', f'{count}x: {text.splitlines()[0]}'))
  return diffs


def compare_logs(old_text: str, new_text: str, diff_preview_limit: int = 40) -> list[Difference]:
  """Returns every Difference found between old_text and new_text, after tolerating the three
  categories of run-to-run noise documented at the top of this module. An empty list means the
  two logs are equivalent for this purpose."""
  old_lines = old_text.splitlines()
  new_lines = new_text.splitlines()

  diffs: list[Difference] = []

  # Excision order (AST, then PROFILE, then messages) is safe against accidentally creating a
  # false continuation-line match in step 3 below: every one of these blocks' own start markers
  # ("AST_DEBUG_BEGIN", "PROFILE", "[TAG:CODE]", "====== HLDB =======") is unindented (column 0)
  # in this log format, so a message's continuation-line scan already stops at any of them in the
  # ORIGINAL text -- removing the block in between two lines that were never going to be joined
  # as one message can't turn them into one after the fact. If a future log format ever puts an
  # indented top-level construct directly after one of these blocks, this assumption should be
  # re-checked.

  # 1. AST_DEBUG blocks -- matched by identity, id-renumbering normalized, then excised so their
  #    (irrelevant) position/count-within-position never affects the leftover comparison below.
  diffs.extend(_compare_ast_blocks(old_lines, new_lines))
  old_ast_blocks, _ = _find_blocks(old_lines, AST_START_RE, AST_END_RE)
  new_ast_blocks, _ = _find_blocks(new_lines, AST_START_RE, AST_END_RE)
  old_lines = _excise_ranges(old_lines, old_ast_blocks)
  new_lines = _excise_ranges(new_lines, new_ast_blocks)

  # 2. PROFILE block -- discarded outright; its content is never compared at all, on either side.
  old_profile_blocks, _ = _find_blocks(old_lines, PROFILE_START_RE, PROFILE_END_RE)
  new_profile_blocks, _ = _find_blocks(new_lines, PROFILE_START_RE, PROFILE_END_RE)
  old_lines = _excise_ranges(old_lines, old_profile_blocks)
  new_lines = _excise_ranges(new_lines, new_profile_blocks)

  # 3. Diagnostic messages -- matched as a multiset, then excised the same way.
  diffs.extend(_compare_messages(old_lines, new_lines))
  old_msg_blocks = [(s, e) for s, e, _ in _message_blocks(old_lines)]
  new_msg_blocks = [(s, e) for s, e, _ in _message_blocks(new_lines)]
  old_lines = _excise_ranges(old_lines, old_msg_blocks)
  new_lines = _excise_ranges(new_lines, new_msg_blocks)

  # 4. Whatever remains (banners, the FATAL/SYNTAX/ERROR/WARNING/NOTE summary, an HLDB dump, or
  #    anything else) must match exactly, in order -- the safety net.
  if old_lines != new_lines:
    diff = list(difflib.unified_diff(old_lines, new_lines, fromfile='old', tofile='new', lineterm=''))
    preview = '\n'.join(diff[:diff_preview_limit])
    if len(diff) > diff_preview_limit:
      preview += f'\n... ({len(diff) - diff_preview_limit} more diff lines omitted)'
    diffs.append(Difference('skeleton-differs', preview))

  return diffs


def logs_equivalent(old_text: str, new_text: str) -> tuple[bool, list[Difference]]:
  """Convenience wrapper: (True, []) if old_text and new_text are equivalent under this module's
  tolerances, else (False, diffs)."""
  diffs = compare_logs(old_text, new_text)
  return (not diffs), diffs


def _main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('old_log', type=Path)
  parser.add_argument('new_log', type=Path)
  args = parser.parse_args(argv)

  old_text = args.old_log.read_text(encoding='utf-8', errors='replace')
  new_text = args.new_log.read_text(encoding='utf-8', errors='replace')

  equivalent, diffs = logs_equivalent(old_text, new_text)
  if equivalent:
    print('EQUIVALENT (no differences outside profile/AST-reordering/message-reordering noise)')
    return 0

  print(f'DIFFERENT: {len(diffs)} difference(s)')
  for d in diffs:
    print(f'  [{d.kind}] {d.detail}')
  return 1


if __name__ == '__main__':
  sys.exit(_main())
