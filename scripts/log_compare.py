#!/usr/bin/env python3

# Copyright 2020 Apotell
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Removes event-order noise from a freshly generated hlc log, relative to the log already on disk.

Events ("[SEV:PPNNNN] ..." blocks with their continuation lines, as split by
log_parser.iter_log_spans()) may come out in a different order from run to run (-mt). The events
both logs hold (same text, counting repeats) are put back in the old log's order: the slots of the
new log that hold them, in file order, are filled with them in the old log's order. An event only
the new log holds stays in its slot. Every other line of the new log is left as is, so any real
difference -- an added, removed or changed event included -- is still there for git diff to show,
without the shared events' reordering around it.

Event text is compared exactly as written (trailing whitespace and '[debug]' lines included);
only line endings are normalized.
"""

import argparse
import sys
from collections import Counter
from pathlib import Path

from log_parser import LogEvent, iter_log_spans


def _split_lines(text: str) -> list[str]:
  return text.replace('\r\n', '\n').split('\n')


def _event_spans(lines: list[str]) -> list[tuple[int, int]]:
  return [(start, end) for item, start, end in iter_log_spans(lines) if isinstance(item, LogEvent)]


def reorder_events(old_text: str, new_text: str) -> str | None:
  """Returns new_text with the events it shares with old_text in old_text's order, or None if
  there is nothing to do: no shared events, or they are already in the same order. The returned
  text uses '\\n' line endings."""
  old_lines = _split_lines(old_text)
  new_lines = _split_lines(new_text)
  if old_lines == new_lines:
    return None  # The common case: nothing changed at all.

  old_events = ['\n'.join(old_lines[start:end]) for start, end in _event_spans(old_lines)]
  new_spans = _event_spans(new_lines)
  new_events = ['\n'.join(new_lines[start:end]) for start, end in new_spans]

  # The events both logs hold, counting repeats. Only these are moved; an event only one of the
  # two logs holds stays where the new log has it.
  common = Counter(old_events) & Counter(new_events)
  if not common:
    return None

  # The common events in the old log's order ...
  remaining = Counter(common)
  old_order = []
  for text in old_events:
    if remaining[text] > 0:
      remaining[text] -= 1
      old_order.append(text)

  # ... and the slots of the new log that hold them, in file order.
  remaining = Counter(common)
  slots = []
  for index, text in enumerate(new_events):
    if remaining[text] > 0:
      remaining[text] -= 1
      slots.append(index)

  if [new_events[index] for index in slots] == old_order:
    return None  # Already in the old log's order.

  for index, text in zip(slots, old_order):
    new_events[index] = text

  result = []
  pos = 0
  for (start, end), text in zip(new_spans, new_events):
    result.extend(new_lines[pos:start])
    result.append(text)
    pos = end
  result.extend(new_lines[pos:])
  return '\n'.join(result)


def _main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('old_log', type=Path)
  parser.add_argument('new_log', type=Path)
  parser.add_argument('-o', '--output', type=Path, help='Write the (possibly reordered) new log here.')
  args = parser.parse_args(argv)

  old_text = args.old_log.read_bytes().decode('utf-8', errors='surrogateescape')
  new_text = args.new_log.read_bytes().decode('utf-8', errors='surrogateescape')

  reordered = reorder_events(old_text, new_text)
  print('Events reordered to match the old log.' if reordered is not None else 'Nothing to reorder.')
  if args.output:
    text = reordered if reordered is not None else new_text
    args.output.write_bytes(text.encode('utf-8', errors='surrogateescape'))
  return 0


if __name__ == '__main__':
  sys.exit(_main())
