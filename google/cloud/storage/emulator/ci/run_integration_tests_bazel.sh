#!/usr/bin/env bash
#
# Copyright 2021 Google LLC
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

source "$(dirname "$0")/../../../../../ci/lib/init.sh"
source module /google/cloud/storage/tools/run_emulator_utils.sh
source module ci/lib/io.sh

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <bazel-program> [bazel-test-args]"
  exit 1
fi

# Configure run_emulators_utils.sh to find the instance admin emulator.
BAZEL_BIN="$1"
shift

# `start_emulator` creates unsightly *.log files in the current directory
# (which is ${PROJECT_ROOT}) and we cannot use a subshell because we want the
# environment variables that it sets.
pushd "${HOME}" >/dev/null
# Start the emulator on a fixed port, otherwise the Bazel cache gets
# invalidated on each run.
start_emulator 8586 8587
popd >/dev/null

set +e
for target in google/cloud/storage/emulator/tests/*.py; do
  io::log_green "Running ${target}"
  target="$(basename "${target}")"
  # TODO(#6641): Remove the retry loop once the flakiness is fixed.
  ci/retry-command.sh 5 0 env -C google/cloud/storage/emulator python -m unittest "tests/${target}"
  exit_status=$?
  if [[ ${exit_status} -ne 0 ]]; then
    break
  fi
done

if [[ "${exit_status}" -ne 0 ]]; then
  io::log_red "emulator log"
  cat "${HOME}/gcs_emulator.log"
fi

exit "${exit_status}"
