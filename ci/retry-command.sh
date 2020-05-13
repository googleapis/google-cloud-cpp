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

set -eu

# Make three attempts to install the dependencies. It is rare, but from time to
# time the downloading the packages, building the Docker images, or an installer
# Bazel, or the Google Cloud SDK fails. To make the CI build more robust, try
# again when that happens.

if (($# < 3)); then
  cat <<EOM
Usage: $(basename "$0") attempts initial-interval program [arguments]

  Retries <program> up to <attempts> times, with exponential backoff.
  The initial retry interval is <initial-interval> seconds.
EOM
  exit 1
fi
retries=${1}
shift
min_wait=${1}
shift
readonly PROGRAM=${1}
shift

# Do not exit on failures for this loop.
set +e
while (((--retries) > 0)); do
  "${PROGRAM}" "$@"
  if [[ $? -eq 0 ]]; then
    exit 0
  fi
  # apply +/- 50% jitter to min_wait
  period=$((min_wait * (50 + (RANDOM % 100)) / 100))
  echo "${PROGRAM} failed; trying again in ${period} seconds."
  sleep ${period}s
  min_wait=$((min_wait * 2))
done

exit 1
