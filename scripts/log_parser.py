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
Splits an hlc log into events, shared by summarize.py and event_coverage.py.

hlc renders an event (hldb's renderEvent()) as one block:

  [SEV:PPNNNN] <file:line:col> <message line 0>,
               <message line 1>,                      <- only if its optional values are given
               In file included from <file>:<line>,   <- context notes, innermost first
               In expansion of macro "<m>" at <file>:<line>:<col>,
               [debug] #0 <type> id=<n>.              <- developer builds only

The parts are joined with ",\n" and the block ends with "."; every line after the first is
indented by the width of the "[SEV:PPNNNN] " prefix. A value may itself span several lines (a
source line and its caret), each indented the same way.

The event table (events.json, see load_event_specs()) tightens the split: a continuation line
must have the header's exact indent, a line ending in "." closes the event, context notes come
after the message lines and only in the kinds the event takes, and an event with a single
message line takes nothing but notes (or the rest of a value spanning lines). Without the table,
or for a code it does not know, only the indent, "." and note-order rules apply. "[debug]" lines
are always dropped; one inside an event is its last part. A line that fits no event is returned
as is, so callers still see coverage lines, reduction results, and the like.
"""

import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Iterator

_re_header = re.compile(r'^\[(?P<sev>[A-Z]{3}):(?P<prefix>[A-Z]{2})(?P<num>\d+)\] ')
_internal_error = 'Internal Error'
_debug_marker = '[debug]'

_severity_tags = {
  'Fatal': 'FTL', 'Syntax': 'SNT', 'Error': 'ERR', 'Warning': 'WRN', 'Info': 'INF', 'Note': 'NTE'
}

# Context notes by the events.json context they belong to. "In file included from the body of
# macro ..." is an include_stack note too, so the longer text is not listed separately.
_note_texts = {
  'include_stack': 'In file included from',
  'macro_stack': 'In expansion of macro',
}


@dataclass
class EventSpec:
  """What events.json says about the layout of one event."""
  m_number: int
  m_name: str
  m_severity: str             # FTL/SNT/ERR/WRN/INF/NTE
  m_prefix: str               # category prefix, e.g. PP
  m_line_count: int           # message lines, the first included
  m_contexts: frozenset       # names from _note_texts the event takes


@dataclass
class LogEvent:
  """One event of a log: its header line and continuation lines, '[debug]' lines left out.
  m_severity, m_prefix and m_number are None for an 'Internal Error' block."""
  m_severity: str | None
  m_prefix: str | None
  m_number: int | None
  m_lines: list = field(default_factory=list)

  @property
  def header(self) -> str:
    return self.m_lines[0]

  @property
  def continuations(self) -> list:
    return self.m_lines[1:]


def load_event_specs(events_filepaths) -> dict:
  """Reads the event tables (share/hldb/events.json, share/hlc/events.json, or a combined one)
  into {code number: EventSpec}."""
  specs = {}
  categories = {}
  for events_filepath in events_filepaths:
    data = json.loads(Path(events_filepath).read_text(encoding='utf-8'))
    for category in data.get('categories', []):
      categories[category['name']] = category['prefix']
    for event in data['events']:
      number = int(event['code'])
      specs[number] = EventSpec(
        m_number=number,
        m_name=event['name'],
        m_severity=_severity_tags.get(event['severity'], ''),
        m_prefix=categories.get(event['category'], ''),
        m_line_count=max(1, len(event.get('message') or [])),
        m_contexts=frozenset(name for name in event.get('context', _note_texts) if name in _note_texts),
      )
  return specs


class _Pending:
  """An event being collected, with what has been seen of it so far."""

  def __init__(self, event: LogEvent, indent: int, spec: EventSpec | None):
    self.m_event = event
    self.m_indent = indent        # 0 for an 'Internal Error' block: any indent continues it
    self.m_spec = spec
    self.m_seen_note = False
    self.m_closed = event.m_lines[0].endswith('.') and (indent > 0)

  def accepts(self, line: str) -> bool:
    if self.m_closed or not line:
      return False
    if self.m_indent == 0:
      return line[:1] in (' ', '\t')
    if (len(line) <= self.m_indent) or line[:self.m_indent].strip():
      return False

    text = line[self.m_indent:]
    note = next((name for name, prefix in _note_texts.items() if text.startswith(prefix)), None)
    if note is not None:
      return (self.m_spec is None) or (note in self.m_spec.m_contexts)
    if self.m_seen_note:
      return False  # Message lines come before the context notes.
    if (self.m_spec is not None) and (self.m_spec.m_line_count == 1):
      # Nothing but notes follows a one-line message, unless the previous line stopped in the
      # middle of a value spanning several lines (it ends in neither ',' nor '.').
      return not self.m_event.m_lines[-1].endswith(',')
    return True

  def add(self, line: str):
    text = line[self.m_indent:]
    if any(text.startswith(prefix) for prefix in _note_texts.values()):
      self.m_seen_note = True
    self.m_event.m_lines.append(line)
    self.m_closed = (self.m_indent > 0) and line.endswith('.')


def _start(line: str, specs: dict | None) -> _Pending | None:
  m = _re_header.match(line)
  if m:
    number = int(m.group('num'))
    spec = specs.get(number) if specs else None
    if (spec is not None) and ((spec.m_severity != m.group('sev')) or (spec.m_prefix != m.group('prefix'))):
      spec = None  # A different numbering (e.g. an older golden log): do not trust the table.
    event = LogEvent(m.group('sev'), m.group('prefix'), number, [line])
    return _Pending(event, m.end(), spec)
  if line.startswith(_internal_error):
    return _Pending(LogEvent(None, None, None, [line]), 0, None)
  return None


def iter_log(lines: Iterable[str], specs: dict | None = None) -> Iterator:
  """Yields each event of the log as a LogEvent and every other line as a str. Lines are
  stripped of trailing whitespace; '[debug]' lines are dropped. specs is load_event_specs()'s
  table, or None to split on the layout alone."""
  pending = None
  for raw in lines:
    line = raw.rstrip()

    if line.lstrip().startswith(_debug_marker):
      # A '[debug]' line inside an event (at its indent) is that event's last part.
      if (pending is not None) and (pending.m_indent > 0) and not line[:pending.m_indent].strip():
        pending.m_closed = True
      continue

    if (pending is not None) and pending.accepts(line):
      pending.add(line)
      continue

    if pending is not None:
      yield pending.m_event
      pending = None

    pending = _start(line, specs)
    if pending is None:
      yield line

  if pending is not None:
    yield pending.m_event
