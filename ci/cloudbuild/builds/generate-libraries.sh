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

if [ -z "${GENERATE_GOLDEN_ONLY}" ]; then
  io::log_h2 "Running the generator to emit protos from discovery docs"
  bazel run --action_env=GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes \
    //generator:google-cloud-cpp-codegen -- \
    --protobuf_proto_path="${bazel_output_base}"/external/com_google_protobuf/src \
    --googleapis_proto_path="${bazel_output_base}"/external/com_google_googleapis \
    --discovery_proto_path="${PWD}" \
    --output_path="${PROJECT_ROOT}" \
    --check_parameter_comment_substitutions=true \
    --generate_discovery_protos=true \
    --config_file="${PROJECT_ROOT}/generator/generator_config.textproto"

  io::log_h2 "Formatting generated protos"
  git ls-files -z -- '*.proto' |
    xargs -P "$(nproc)" -n 1 -0 clang-format -i
else
  io::log_red "Skipping update of protos generated from discovery docs."
fi

if [ -z "${GENERATE_GOLDEN_ONLY}" ]; then
  io::log_h2 "Running the generator to update the generated libraries"
  bazel run --action_env=GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes \
    //generator:google-cloud-cpp-codegen -- \
    --protobuf_proto_path="${bazel_output_base}"/external/com_google_protobuf/src \
    --googleapis_proto_path="${bazel_output_base}"/external/com_google_googleapis \
    --discovery_proto_path="${PWD}" \
    --output_path="${PROJECT_ROOT}" \
    --check_parameter_comment_substitutions=true \
    --config_file="${PROJECT_ROOT}/generator/generator_config.textproto"
else
  io::log_red "Skipping update of generated libraries."
fi

if [ -z "${GENERATE_GOLDEN_ONLY}" ]; then
  # TODO(#5821): The generator should run clang-format on its output files itself
  # so we don't need this extra step.
  io::log_h2 "Formatting generated code"
  git ls-files -z -- '*.h' '*.cc' '*.proto' |
    xargs -P "$(nproc)" -n 1 -0 clang-format -i
  git ls-files -z -- '*.h' '*.cc' '*.proto' |
    xargs -r -P "$(nproc)" -n 50 -0 sed -i 's/[[:blank:]]\+$//'
else
  io::log_red "Only formatting generated golden code."
  git ls-files -z -- 'generator/integration_tests/golden/**/*.h' \
    'generator/integration_tests/golden/v1/**/*.cc' \
    'generator/integration_tests/*.proto' |
    xargs -P "$(nproc)" -n 1 -0 clang-format -i
  git ls-files -z -- 'generator/integration_tests/golden/**/*.h' \
    'generator/integration_tests/golden/v1/**/*.cc' \
    'generator/integration_tests/*.proto' |
    xargs -r -P "$(nproc)" -n 50 -0 sed -i 's/[[:blank:]]\+$//'
fi

if [[ "${TRIGGER_TYPE:-}" != "manual" ]]; then
  io::log_h2 "Updating protobuf lists/deps"
  external/googleapis/update_libraries.sh
else
  io::log_yellow "Skipping update of protobuf lists/deps."
fi

# This build should fail if any generated files differ from what was checked in.
io::log_h2 "Highlight generated code differences"
# We use `--compact-summary` because in almost all cases the delta is at
# least hundreds of lines long, and often it is thousands of lines long. The
# summary is all we need in the script.  The developer can do specific diffs
# if needed.
#
# We only compare a subset of the directories because we expect changes in
# generator/... (but not in generator/integration_tests/golden) while making
# generator changes.
git diff --exit-code --compact-summary generator/integration_tests/golden/ google/ ci/

if [[ -n "$(git status --porcelain)" ]]; then
  io::log_red "New unmanaged files created by generator"
  git status
  exit 1
fi
