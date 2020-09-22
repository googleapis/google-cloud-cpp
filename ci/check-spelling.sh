#!/usr/bin/env bash
# Copyright 2020 Google LLC
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

source "$(dirname "$0")/lib/init.sh"
source module lib/io.sh

if [[ "${CHECK_SPELLING}" != "yes" ]]; then
  echo "Skipping the spelling check as it is disabled for this build."
  exit 0
fi

# This script must run from the top-level google-cloud-cpp directory.
cd "${PROJECT_ROOT}"

# This controls the output format from bash's `time` command, which we use
# below to time blocks of the script. A newline is automatically included.
readonly TIMEFORMAT="... %R seconds"

exit_status=0
printf "%-30s" "Running cspell:"
time {
  if ! git ls-files -z | grep -zE '\.(h)$' |
        xargs -P "${NCPU}" -n 50 -0 cspell -c ci/cspell.json; then
      io::log_red "Detected spelling problems"
      exit_status=1
  fi
}

exit ${exit_status}
