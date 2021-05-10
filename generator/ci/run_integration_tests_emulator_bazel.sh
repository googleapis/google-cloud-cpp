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

source "$(dirname "$0")/../../ci/lib/init.sh"
source module /ci/etc/integration-tests-config.sh

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <bazel-program> [bazel-test-args]"
  exit 1
fi

BAZEL_BIN="$1"
shift
BAZEL_VERB="$1"
shift
bazel_test_args=("$@")

readonly GOOGLE_CLOUD_CPP_GENERATOR_GOOGLEAPIS_PATH=$(bazel info output_base)"/external/com_google_googleapis/"
readonly GOOGLE_CLOUD_CPP_GENERATOR_PROTO_PATH=$(bazel info output_base)"/external/com_google_protobuf/src/"

"${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" \
  --test_env="GOOGLE_CLOUD_CPP_GENERATOR_GOOGLEAPIS_PATH=${GOOGLE_CLOUD_CPP_GENERATOR_GOOGLEAPIS_PATH}" \
  --test_env="GOOGLE_CLOUD_CPP_GENERATOR_PROTO_PATH=${GOOGLE_CLOUD_CPP_GENERATOR_PROTO_PATH}" \
  --test_env="GOOGLE_CLOUD_CPP_GENERATOR_RUN_INTEGRATION_TESTS=${GOOGLE_CLOUD_CPP_GENERATOR_RUN_INTEGRATION_TESTS}" \
  --test_env="GOOGLE_CLOUD_CPP_GENERATOR_CODE_PATH=${GOOGLE_CLOUD_CPP_GENERATOR_CODE_PATH:-/v}" \
  --test_tag_filters="integration-test" -- \
  "//generator/..."
exit_status=$?

exit "${exit_status}"
