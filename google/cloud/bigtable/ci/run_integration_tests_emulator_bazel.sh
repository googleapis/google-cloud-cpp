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

source "$(dirname "$0")/../../../../ci/lib/init.sh"

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <bazel-program> [bazel-test-args]"
  exit 1
fi

BAZEL_BIN="$1"
shift
BAZEL_VERB="$1"
shift
bazel_test_args=("$@")

# Configure run_emulators_utils.sh to find the instance admin emulator.
BAZEL_BIN_DIR="$("${BAZEL_BIN}" info bazel-bin)"
readonly BAZEL_BIN_DIR
export CBT_INSTANCE_ADMIN_EMULATOR_CMD="${BAZEL_BIN_DIR}/google/cloud/bigtable/tests/instance_admin_emulator"
source module /google/cloud/bigtable/tools/run_emulator_utils.sh

# These can only run against production
production_only_targets=(
  "//google/cloud/bigtable/examples:table_admin_iam_policy_snippets"
  "//google/cloud/bigtable/tests:admin_iam_policy_integration_test"
  "//google/cloud/bigtable/tests:admin_backup_integration_test"
  "//google/cloud/bigtable/examples:bigtable_table_admin_backup_snippets"
)
"${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" \
  --test_tag_filters="integration-test" -- \
  "${production_only_targets[@]}"

# `start_emulators` creates unsightly *.log files in the current directory
# (which is ${PROJECT_ROOT}) and we cannot use a subshell because we want the
# environment variables that it sets.
pushd "${HOME}" >/dev/null
start_emulators 8480 8490
popd >/dev/null

excluded_targets=(
  # This test can only run against production, *and* needs dynamically created
  # environment variables, it is called from build-in-docker-bazel.sh
  "-//google/cloud/bigtable/examples:bigtable_grpc_credentials"
)
for target in "${production_only_targets[@]}"; do
  excluded_targets+=("-${target}")
done

"${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" \
  --test_env="BIGTABLE_EMULATOR_HOST=${BIGTABLE_EMULATOR_HOST}" \
  --test_env="BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST=${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST}" \
  --test_tag_filters="integration-test" -- \
  "//google/cloud/bigtable/..." \
  "${excluded_targets[@]}"
exit_status=$?

kill_emulators
trap '' EXIT

exit "${exit_status}"
