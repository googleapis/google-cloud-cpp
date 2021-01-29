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
source module lib/io.sh

if [[ "${GENERATE_LIBRARIES}" != "yes" ]]; then
  echo "Skipping the generated code hash check as it is disabled for this build."
  exit 0
fi

readonly BAZEL_BIN=${BAZEL_BIN:-/usr/local/bin/bazel}
readonly BAZEL_OUTPUT_BASE=$("${BAZEL_BIN}" info output_base)
readonly BAZEL_BIN_DIR=$("${BAZEL_BIN}" info bazel-bin)
readonly BAZEL_DEPS_GOOGLEAPIS_HASH=$("${BAZEL_BIN}" query 'kind(http_archive, //external:com_google_googleapis)' --output=build | sed -n 's/^.*strip_prefix = "googleapis-\(\S*\)"\,.*$/\1/p')

io::log_yellow "Build protoc binary"
${BAZEL_BIN} build @com_google_protobuf//:protoc

io::log_yellow "Build microgenerator plugin"
${BAZEL_BIN} build //generator:protoc-gen-cpp_codegen

product_path_proto_path_to_generate=(
  "2020 google/cloud/iam google/iam/credentials/v1/iamcredentials.proto"
  "2021 google/cloud/logging google/logging/v2/logging.proto"
)

io::log_yellow "Run protoc and format generated .h and .cc files:"
cd "${PROJECT_ROOT}"
for proto in "${product_path_proto_path_to_generate[@]}"; do
  read -r -a tuple <<<"${proto}"
  copyright_year=${tuple[0]}
  product_path=${tuple[1]}
  proto_file=${tuple[2]}
  echo "Generate code from ${proto_file} to ${product_path}"
  GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes "${BAZEL_BIN_DIR}"/external/com_google_protobuf/protoc \
    --plugin=protoc-gen-cpp_codegen="${BAZEL_BIN_DIR}"/generator/protoc-gen-cpp_codegen \
    --cpp_codegen_out=. \
    --cpp_codegen_opt=product_path="${product_path}" \
    --proto_path="${BAZEL_OUTPUT_BASE}"/external/com_google_protobuf/src \
    --proto_path="${BAZEL_OUTPUT_BASE}"/external/com_google_googleapis \
    --cpp_codegen_opt=googleapis_commit_hash="${BAZEL_DEPS_GOOGLEAPIS_HASH}" \
    --cpp_codegen_opt=copyright_year="${copyright_year}" \
    "${BAZEL_OUTPUT_BASE}"/external/com_google_googleapis/"${proto_file}"

  find "${product_path}" \( -name '*.cc' -o -name '*.h' \) -exec clang-format -i {} \;
done

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

exit 0
