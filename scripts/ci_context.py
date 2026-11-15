#!/usr/bin/env python

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
Print a sanitized description of the CI run as JSON (written to env.json).

This replaces dumping ${{ toJson(github) }}, which embeds github.token (the
job's GITHUB_TOKEN) and the full event payload. Only allowlisted, non-secret
fields are emitted. Add new fields here explicitly -- never dump whole
contexts or os.environ.
"""

import json
import os
import sys

_env_keys = [
  'GITHUB_REPOSITORY',
  'GITHUB_REF',
  'GITHUB_REF_NAME',
  'GITHUB_HEAD_REF',
  'GITHUB_BASE_REF',
  'GITHUB_SHA',
  'GITHUB_WORKFLOW',
  'GITHUB_JOB',
  'GITHUB_RUN_ID',
  'GITHUB_RUN_NUMBER',
  'GITHUB_RUN_ATTEMPT',
  'GITHUB_EVENT_NAME',
  'GITHUB_SERVER_URL',
  'RUNNER_OS',
  'RUNNER_ARCH',
  'RUNNER_NAME',
]

# The hlc build run under test, exported by the hlc workflow's Summarize job. Recorded under
# 'hlc' so the summary still names the hlc commit once that run (and its artifacts) are deleted.
_hlc_env_keys = {
  'HLC_RUN_ID': 'run_id',
  'HLC_RUN_NUMBER': 'run_number',
  'HLC_RUN_ATTEMPT': 'run_attempt',
  'HLC_BRANCH': 'branch',
  'HLC_SHA': 'sha',
}


def _event_summary() -> dict:
  event_path = os.environ.get('GITHUB_EVENT_PATH')
  if not event_path or not os.path.isfile(event_path):
    return {}

  with open(event_path, encoding='utf-8') as strm:
    event = json.load(strm)

  summary = {}
  # workflow_dispatch inputs are declared in the workflow files and hold no secrets.
  if isinstance(event.get('inputs'), dict):
    summary['inputs'] = event['inputs']

  pull_request = event.get('pull_request')
  if isinstance(pull_request, dict):
    summary['pull_request'] = {
      'number': pull_request.get('number'),
      'head_sha': (pull_request.get('head') or {}).get('sha'),
      'head_ref': (pull_request.get('head') or {}).get('ref'),
      'base_sha': (pull_request.get('base') or {}).get('sha'),
      'base_ref': (pull_request.get('base') or {}).get('ref'),
    }
  return summary


def ci_context() -> dict:
  context = {key: os.environ[key] for key in _env_keys if key in os.environ}
  context['event'] = _event_summary()

  hlc = {name: os.environ[key] for key, name in _hlc_env_keys.items() if os.environ.get(key)}
  if hlc:
    context['hlc'] = hlc
  return context


def main() -> int:
  json.dump(ci_context(), sys.stdout, indent=2)
  sys.stdout.write('\n')
  return 0


if __name__ == '__main__':
  sys.exit(main())
