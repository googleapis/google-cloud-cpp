#!/bin/bash
#
# Copyright 2021 Google LLC
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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/git.sh

bazel_output_base="$(bazel info output_base)"

io::log_h2 "Running the generator to update the golden files"
bazel run --action_env=GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes \
  //generator:google-cloud-cpp-codegen -- \
  --protobuf_proto_path="${bazel_output_base}/external/com_google_protobuf/src" \
  --googleapis_proto_path="${bazel_output_base}/external/com_google_googleapis" \
  --golden_proto_path="${PWD}" \
  --output_path="${PWD}" \
  --update_ci=false \
  --config_file="${PWD}/generator/integration_tests/golden_config.textproto"

io::log_h2 "Running the generator to update the generated libraries"
bazel run --action_env=GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes \
  //generator:google-cloud-cpp-codegen -- \
  --protobuf_proto_path="${bazel_output_base}"/external/com_google_protobuf/src \
  --googleapis_proto_path="${bazel_output_base}"/external/com_google_googleapis \
  --output_path="${PROJECT_ROOT}" \
  --config_file="${PROJECT_ROOT}/generator/generator_config.textproto"

# TODO(#5821): The generator should run clang-format on its output files itself
# so we don't need this extra step.
io::log_h2 "Formatting generated code"
git ls-files -z | grep -zE '\.(cc|h)$' |
  xargs -P "$(nproc)" -n 1 -0 clang-format -i

io::log_h2 "Updating protobuf lists/deps"
external/googleapis/update_libraries.sh

# This build should fail if any generated files differ from what was checked
# in. We only look in the google/ directory so that the build doesn't fail if
# it's run while editing files in generator/...
io::log_h2 "Highlight generated code differences"
git diff --exit-code generator/integration_tests/golden/ google/ ci/

io::log_h2 "Highlight new files"
if [[ -n "$(git status --porcelain)" ]]; then
  io::log_red "New unmanaged files created by generator"
  git status
  exit 1
fi
