#!/usr/bin/env bash
#
# Copyright 2018 Google LLC
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

echo "Reading CI secret configuration parameters."
source "${KOKORO_GFILE_DIR}/test-configuration.sh"

echo "Running build and tests"
cd "$(dirname $0)/../../.."
readonly PROJECT_ROOT="${PWD}"

cat >>kokoro-bazelrc <<_EOF_
# Set flags for uploading to BES without Remote Build Execution.
startup --host_jvm_args=-Dbazel.DigestFunction=SHA256
build:results-local --remote_cache=remotebuildexecution.googleapis.com
build:results-local --spawn_strategy=local
build:results-local --remote_timeout=3600
build:results-local --bes_backend="buildeventservice.googleapis.com"
build:results-local --bes_best_effort=false
build:results-local --bes_timeout=10m
build:results-local --tls_enabled=true
build:results-local --auth_enabled=true
build:results-local --auth_scope=https://www.googleapis.com/auth/cloud-source-tools
build:results-local --experimental_remote_spawn_cache
_EOF_

# Kokoro does guarantee that g++-4.9 will be installed, but the default compiler
# might be g++-4.8. Set the compiler version explicitly:
export CC=/usr/bin/gcc-4.9
export CXX=/usr/bin/g++-4.9

# First build and run the unit tests.
readonly INVOCATION_ID="$(python -c 'import uuid; print uuid.uuid4()')"
echo "Configure and start Bazel: " ${INVOCATION_ID}
echo "================================================================"
echo "https://source.cloud.google.com/results/invocations/${INVOCATION_ID}"
echo "================================================================"
echo ${INVOCATION_ID} >> "${KOKORO_ARTIFACTS_DIR}/bazel_invocation_ids"

bazel test \
    --test_output=errors \
    --verbose_failures=true \
    --keep_going \
    -- //google/cloud/...:all

echo
echo "================================================================"
echo "================================================================"
echo "Copying artifacts"
cp "$(bazel info output_base)/java.log" "${KOKORO_ARTIFACTS_DIR}/" || echo "java log copy failed."
echo "End of copying."

echo
echo "================================================================"
echo "================================================================"
# Then build everything else (integration tests, examples, etc). So we can run
# them next.
bazel build \
    --test_output=errors \
    --verbose_failures=true \
    --keep_going \
    -- //google/cloud/...:all

# The integration tests need further configuration and tools.
echo
echo "================================================================"
echo "================================================================"

echo "Download dependencies for integration tests."
# Download the gRPC `roots.pem` file. Somewhere inside the bowels of Bazel, this
# file might exist, but my attempts at using it have failed.
echo "    Getting roots.pem for gRPC."
wget -q https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem
echo "    Getting cbt tool"
export GOPATH="${KOKORO_ROOT}/golang"
go get -u cloud.google.com/go/bigtable/cmd/cbt
echo "End of download."

export GOOGLE_APPLICATION_CREDENTIALS="${KOKORO_GFILE_DIR}/service-account.json"
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="$PWD/roots.pem"

# If this file does not exist gRPC blocks trying to connect, so it is better
# to break the build early (the ls command breaks and the build stops) if that
# is the case.
echo "GRPC_DEFAULT_SSL_ROOTS_FILE_PATH = ${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}"
ls -l "$(dirname "${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}")"
ls -l "${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}"

echo
echo "================================================================"
echo "================================================================"
echo "Running Google Cloud Bigtable Integration Tests"
(cd $(bazel info bazel-bin)/google/cloud/bigtable/tests && \
   "${PROJECT_ROOT}/google/cloud/bigtable/tests/run_integration_tests_production.sh")
(cd $(bazel info bazel-bin)/google/cloud/bigtable/examples && \
   "${PROJECT_ROOT}/google/cloud/bigtable/examples/run_examples_production.sh")

echo "Running Google Cloud Storage Integration Tests"
(cd $(bazel info bazel-bin)/google/cloud/storage/tests && \
    "${PROJECT_ROOT}/google/cloud/storage/tests/run_integration_tests_production.sh")
echo "Running Google Cloud Storage Examples"
(cd $(bazel info bazel-bin)/google/cloud/storage/examples && \
    "${PROJECT_ROOT}/google/cloud/storage/examples/run_examples_production.sh")
