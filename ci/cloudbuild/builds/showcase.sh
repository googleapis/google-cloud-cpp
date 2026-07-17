#!/bin/bash
#
# Copyright 2026 Google LLC
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
source module ci/cloudbuild/builds/lib/cloudcxxrc.sh
source module ci/lib/io.sh

export CC=clang
export CXX=clang++

mapfile -t args < <(bazel::common_args)

io::log_h1 "Running Showcase PQC tests with Bazel"

# Ensure server and temporary files are cleaned up on script exit
cleanup() {
  rm -f ci/showcase/BUILD.bazel
  if [[ -n "${showcase_pid:-}" ]]; then
    kill "${showcase_pid}" || true
  fi
}
trap cleanup EXIT

SHOWCASE_VERSION="${SHOWCASE_VERSION:-main}"
SHOWCASE_DIR="ci/showcase/googleapis/gapic-showcase"
if [[ ! -f "${SHOWCASE_DIR}/go.mod" ]]; then
  io::log_h2 "Downloading googleapis/gapic-showcase (${SHOWCASE_VERSION}) tarball into ${SHOWCASE_DIR}"
  mkdir -p "${SHOWCASE_DIR}"
  curl -fsSL "https://github.com/googleapis/gapic-showcase/archive/${SHOWCASE_VERSION}.tar.gz" |
    tar -C "${SHOWCASE_DIR}" -xzf - --strip-components=1
fi

# Copy BUILD.bazel.in to BUILD.bazel. These shenanigans are necessary because
# we want to test with packages that do not have public visibility and creating
# a persistent BUILD.bazel file leads to errors when the files it refers to
# are not there.
cp ci/showcase/BUILD.bazel.in ci/showcase/BUILD.bazel

bazel_output_base="$(bazel info output_base)"
protobuf_proto_path="${bazel_output_base}/external/protobuf+/src"
googleapis_proto_path="${bazel_output_base}/external/googleapis+"

io::log_h2 "Running C++ codegen generator for gapic-showcase echo.proto"
bazel run --action_env=GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes \
  //generator:google-cloud-cpp-codegen -- \
  --protobuf_proto_path="${protobuf_proto_path}" \
  --googleapis_proto_path="${googleapis_proto_path}" \
  --golden_proto_path="${PROJECT_ROOT}/${SHOWCASE_DIR}/schema" \
  --output_path="${PROJECT_ROOT}/ci/showcase" \
  --check_comment_substitutions=false \
  --config_file="${PROJECT_ROOT}/ci/showcase/showcase_config.textproto"

io::log_h2 "Building gapic-showcase server binary"
mkdir -p "${PROJECT_ROOT}/ci/showcase/bin"
(
  cd "${PROJECT_ROOT}/${SHOWCASE_DIR}"
  go build -o "${PROJECT_ROOT}/ci/showcase/bin/gapic-showcase" ./cmd/gapic-showcase
)

SHOWCASE_PORT="${SHOWCASE_PORT:-7469}"
SHOWCASE_CA_CERT="${PROJECT_ROOT}/ci/showcase/showcase.pem"
rm -f "${SHOWCASE_CA_CERT}"

io::log_h2 "Starting gapic-showcase server on port ${SHOWCASE_PORT} with TLS"
"${PROJECT_ROOT}/ci/showcase/bin/gapic-showcase" run \
  --port=":${SHOWCASE_PORT}" \
  --tls \
  --ca-cert-output-file="${SHOWCASE_CA_CERT}" >/dev/null 2>&1 &
showcase_pid=$!

# Wait up to 15 seconds for CA cert file to be created and server to be listening
for _ in $(seq 1 30); do
  if [[ -r "${SHOWCASE_CA_CERT}" ]] && curl --insecure -s "https://localhost:${SHOWCASE_PORT}" >/dev/null 2>&1; then
    break
  fi
  sleep 0.5
done

io::log_h2 "Running showcase tests"
bazel test --test_env=SHOWCASE_PORT="${SHOWCASE_PORT}" \
  --test_env=SHOWCASE_CA_CERT="${SHOWCASE_CA_CERT}" \
  --test_output=errors \
  //ci/showcase:rest_pqc_test
