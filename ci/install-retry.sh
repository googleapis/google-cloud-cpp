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

if (( $# < 1 )); then
  echo "Usage: $(basename "$0") program [arguments]"
  exit 1
fi
readonly PROGRAM=${1}
shift

# Initially, wait at least 2 minutes (the times are in seconds), because it
# makes no sense to try faster. This used to be 180 seconds, but that ends with
# sleeps close to 10 minutes, and Travis aborts builds that do not output in
# 10m.
min_wait=120
# Do not exit on failures for this loop.
set +e
for i in 1 2 3; do
  "${PROGRAM}" "$@"
  if [[ $? -eq 0 ]]; then
    exit 0
  fi
  # Sleep for a few minutes before trying again.
  period=$(( (RANDOM % 60) + min_wait ))
  echo "${PROGRAM} failed; trying again in ${period} seconds."
  sleep ${period}s
  min_wait=$(( min_wait * 2 ))
done

exit 1
