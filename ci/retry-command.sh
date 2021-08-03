#!/usr/bin/env bash
#
# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

if (($# < 3)); then
  cat <<EOM
Usage: $(basename "$0") retries initial-interval program [arguments]

  Runs <program> [arguments], and then retries up to <retries> times,
  with jittered exponential backoff between failures. The initial backoff
  interval is <initial-interval> seconds.
EOM
  exit 1
fi
retries=${1}
shift
min_wait=${1}
shift
readonly PROGRAM=${1}
shift

"${PROGRAM}" "$@" && exit 0
status=${?}

while (((retries--) > 0)); do
  # apply +/- 50% jitter to min_wait
  period=$((min_wait * (50 + (RANDOM % 100)) / 100))
  echo "${PROGRAM} failed; trying again in ${period} seconds."
  sleep ${period}s
  min_wait=$((min_wait * 2))
  "${PROGRAM}" "$@" && exit 0
  status=${?}
done

exit "${status}"
