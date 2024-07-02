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
  --protobuf_proto_path="${bazel_output_base}/external/protobuf~/src" \
  --googleapis_proto_path="${bazel_output_base}/external/googleapis~" \
  --golden_proto_path="${PWD}" \
  --output_path="${PWD}" \
  --update_ci=false \
  --config_file="${PWD}/generator/integration_tests/golden_config.textproto"

if [ -z "${GENERATE_GOLDEN_ONLY}" ]; then
  newer_tmp_file="$(mktemp)"

  io::log_h2 "Running the generator to emit protos from discovery docs"
  bazel run --action_env=GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes \
    //generator:google-cloud-cpp-codegen -- \
    --protobuf_proto_path="${bazel_output_base}"/external/protobuf~/src \
    --googleapis_proto_path="${bazel_output_base}"/external/googleapis~ \
    --discovery_proto_path="${PWD}/protos" \
    --output_path="${PROJECT_ROOT}/protos" \
    --export_output_path="${PROJECT_ROOT}" \
    --check_comment_substitutions=true \
    --generate_discovery_protos=true \
    --config_file="${PROJECT_ROOT}/generator/generator_config.textproto"

  io::log_h2 "Removing obsolete google/cloud/compute/v1/internal/*.proto"
  find "${PROJECT_ROOT}/protos/google/cloud/compute/v1/internal/" -name '*.proto' -a ! -newer "${newer_tmp_file}" -exec git rm {} \;
  rm "${newer_tmp_file}"

  io::log_h2 "Formatting and adding any new google/cloud/compute/v1/internal/common_*.proto"
  find "${PROJECT_ROOT}/protos/google/cloud/compute/v1/internal" -name '*.proto' -exec clang-format -i {} \;
  git add "${PROJECT_ROOT}/protos/google/cloud/compute/v1/internal/common_*.proto"

  if [ -n "${UPDATED_DISCOVERY_DOCUMENT}" ]; then
    io::log_h2 "Adding new ${UPDATED_DISCOVERY_DOCUMENT} internal protos"
    git add "${PROJECT_ROOT}/protos/google/cloud/${UPDATED_DISCOVERY_DOCUMENT}/v1/internal"
  fi

  io::log_h2 "Formatting generated protos"
  git ls-files -z -- '*.proto' |
    xargs -P "$(nproc)" -n 50 -0 clang-format -i
else
  io::log_red "Skipping update of protos generated from discovery docs."
fi

if [ -z "${GENERATE_GOLDEN_ONLY}" ]; then
  io::log_h2 "Running the generator to update the generated libraries"
  bazel run --action_env=GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes \
    //generator:google-cloud-cpp-codegen -- \
    --protobuf_proto_path="${bazel_output_base}"/external/protobuf~/src \
    --googleapis_proto_path="${bazel_output_base}"/external/googleapis~ \
    --discovery_proto_path="${PWD}/protos" \
    --output_path="${PROJECT_ROOT}" \
    --check_comment_substitutions=true \
    --config_file="${PROJECT_ROOT}/generator/generator_config.textproto"

  if [ -n "${UPDATED_DISCOVERY_DOCUMENT}" ]; then
    io::log_h2 "Adding new ${UPDATED_DISCOVERY_DOCUMENT} generated service code"
    git add "${PROJECT_ROOT}/google/cloud/${UPDATED_DISCOVERY_DOCUMENT}"
  fi
else
  io::log_red "Skipping update of generated libraries."
fi

if [ -z "${GENERATE_GOLDEN_ONLY}" ]; then
  # TODO(#5821): The generator should run clang-format on its output files itself
  # so we don't need this extra step.
  io::log_h2 "Formatting generated code"
  git ls-files -z -- '*.h' '*.cc' '*.proto' |
    xargs -P "$(nproc)" -n 50 -0 clang-format -i
  git ls-files -z -- '*.h' '*.cc' '*.proto' |
    xargs -r -P "$(nproc)" -n 50 -0 sed -i 's/[[:blank:]]\+$//'
  # TODO(#12621): Remove this second execution of clang-format when this issue
  # is resolved.
  git ls-files -z -- 'google/cloud/compute/firewall_policies/v1/internal/firewall_policies_rest_connection_impl.cc' |
    xargs -P "$(nproc)" -n 50 -0 clang-format -i
else
  io::log_red "Only formatting generated golden code."
  git ls-files -z -- 'generator/integration_tests/golden/**/*.h' \
    'generator/integration_tests/golden/v1/**/*.cc' \
    'generator/integration_tests/*.proto' |
    xargs -P "$(nproc)" -n 50 -0 clang-format -i
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

if [ -n "${UPDATED_DISCOVERY_DOCUMENT}" ]; then
  io::log_h2 "Adding new ${UPDATED_DISCOVERY_DOCUMENT} files post formatting"
  git add -u "${PROJECT_ROOT}/google/cloud/${UPDATED_DISCOVERY_DOCUMENT}" \
    "${PROJECT_ROOT}/protos/google/cloud/${UPDATED_DISCOVERY_DOCUMENT}" \
    "${PROJECT_ROOT}/ci/etc/expected_install_directories"
else
  # If the Discovery Document is not being updated, then this build should fail
  # if any generated files differ from what was checked in.
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
fi
