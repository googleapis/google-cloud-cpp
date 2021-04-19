#!/usr/bin/env bash
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

source "$(dirname "$0")/../ci/lib/init.sh"
source module /ci/lib/io.sh

if [[ "${GENERATE_LIBRARIES}" != "yes" ]]; then
  echo "Skipping the generated code hash check as it is disabled for this build."
  exit 0
fi

readonly BAZEL_BIN=${BAZEL_BIN:-/usr/local/bin/bazel}
readonly BAZEL_OUTPUT_BASE=$("${BAZEL_BIN}" info output_base)
readonly BAZEL_BIN_DIR=$("${BAZEL_BIN}" info bazel-bin)
readonly BAZEL_DEPS_GOOGLEAPIS_HASH=$("${BAZEL_BIN}" query \
  'kind(http_archive, //external:com_google_googleapis)' --output=build |
  sed -n 's/^.*strip_prefix = "googleapis-\(\S*\)"\,.*$/\1/p')

io::log_yellow "Build protoc binary"
${BAZEL_BIN} build @com_google_protobuf//:protoc

io::log_yellow "Build microgenerator standalone executable"
${BAZEL_BIN} build //generator:protoc-gen-cpp_codegen

io::log_yellow "Run protoc and format generated .h and .cc files:"
cd "${PROJECT_ROOT}"
GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes "${BAZEL_BIN_DIR}"/generator/google-cloud-cpp-codegen \
  --googleapis_commit_hash="${BAZEL_DEPS_GOOGLEAPIS_HASH}" \
  --protobuf_proto_path="${BAZEL_OUTPUT_BASE}"/external/com_google_protobuf/src \
  --googleapis_proto_path="${BAZEL_OUTPUT_BASE}"/external/com_google_googleapis \
  --output_path=. \
  --config_file=generator/generator_config.textproto

find google/cloud \( -name '*.cc' -o -name '*.h' \) -exec clang-format -i {} \;

io::log_yellow "Update generator-googlapis-commit-hash.sh file."
cat >"${PROJECT_ROOT}"/ci/etc/generator-googleapis-commit-hash.sh <<EOF
#!/usr/bin/env bash
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

GOOGLEAPIS_HASH_LAST_USED=$BAZEL_DEPS_GOOGLEAPIS_HASH
readonly GOOGLEAPIS_HASH_LAST_USED
EOF

io::log_yellow "Update ci/etc/generator-golden-md5-hashes.md5."
find generator/integration_tests/golden/ \
  -path generator/integration_tests/golden/tests -prune -false -o \
  -name '*golden*.h' -print0 -o -name '*golden*.cc' -print0 | sort -z |
  xargs -0 md5sum >"${PROJECT_ROOT}"/ci/etc/generator-golden-md5-hashes.md5
exit 0
