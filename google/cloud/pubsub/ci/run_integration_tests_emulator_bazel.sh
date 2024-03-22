#!/usr/bin/env bash
#
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

source "$(dirname "$0")/../../../../ci/lib/init.sh"
source module /ci/cloudbuild/builds/lib/cloudcxxrc.sh
source module /google/cloud/pubsub/ci/lib/pubsub_emulator.sh

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <bazel-program> [bazel-test-args]"
  exit 1
fi

BAZEL_BIN="$1"
shift
BAZEL_VERB="$1"
shift
bazel_test_args=("$@")

if bazel::has_no_tests "//google/cloud/pubsub/..."; then
  exit 0
fi

# These can only run against production
production_only_targets=(
  "//google/cloud/pubsub/samples:iam_samples"
)

# Coverage builds are more subject to flakiness, as we must explicitly disable
# retries. Disable the production-only tests, which also fail more often.
if [[ "${BAZEL_VERB}" != "coverage" ]]; then
  io::run "${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" \
    -- "${production_only_targets[@]}"
fi

# Start the emulator and arranges to kill it, run in $HOME because
# pubsub_emulator::start creates unsightly *.log files in the workspace
# otherwise.
pushd "${HOME}" >/dev/null
pubsub_emulator::start
popd >/dev/null
trap pubsub_emulator::kill EXIT

excluded_targets=()
for target in "${production_only_targets[@]}"; do
  excluded_targets+=("-${target}")
done

"${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" \
  --test_env="PUBSUB_EMULATOR_HOST=${PUBSUB_EMULATOR_HOST}" \
  -- "//google/cloud/pubsub/...:all" "${excluded_targets[@]}"
