#!/bin/bash
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

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/git.sh

bazel_output_base="$(bazel info output_base)"
googleapis_hash=$(bazel query \
  'kind(http_archive, //external:com_google_googleapis)' --output=build |
  sed -n 's/^.*strip_prefix = "googleapis-\(\S*\)"\,.*$/\1/p')

io::log_h2 "Running generator"
bazel run --action_env=GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes \
  //generator:google-cloud-cpp-codegen -- \
  --googleapis_commit_hash="${googleapis_hash}" \
  --protobuf_proto_path="${bazel_output_base}"/external/com_google_protobuf/src \
  --googleapis_proto_path="${bazel_output_base}"/external/com_google_googleapis \
  --output_path="${PROJECT_ROOT}" \
  --config_file="${PROJECT_ROOT}/generator/generator_config.textproto"

# TODO(#5821): The generator should run clang-format on its output files itself
# so we don't need this extra step.
io::log_h2 "Formatting generated code"
find google/cloud -path google/cloud/bigtable -prune -o \
  -path google/cloud/examples -prune -o \
  -path google/cloud/firestore -prune -o \
  -path google/cloud/grpc_utils -prune -o \
  -path google/cloud/internal -prune -o \
  -path google/cloud/pubsub -prune -o \
  -path google/cloud/spanner -prune -o \
  -path google/cloud/storage -prune -o \
  -path google/cloud/testing_util -prune -o \
  \( -name '*.cc' -o -name '*.h' \) -exec clang-format -i {} \;

# This build should fail if any generated files differ from what was checked
# in. We only look in the google/ directory so that the build doesn't fail if
# it's run while editing files in generator/...
git diff --exit-code google/
