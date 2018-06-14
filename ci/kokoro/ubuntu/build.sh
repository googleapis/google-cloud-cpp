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

# First build and run the unit tests.
INVOCATION_ID="$(python -c 'import uuid; print uuid.uuid4()')"
echo "Configure and start Bazel: " ${INVOCATION_ID}
echo "================================================================"
echo "    https://source.cloud.google.com/results/invocations/${INVOCATION_ID}"
echo "================================================================"
echo ${INVOCATION_ID} >> "${KOKORO_ARTIFACTS_DIR}/bazel_invocation_ids"

bazel --bazelrc=kokoro-bazelrc test \
    --test_output=errors \
    --verbose_failures=true \
    --keep_going \
    --project_id=${KOKORO_PROJECT_ID} \
    --auth_credentials="${KOKORO_GFILE_DIR}/build-results-service-account.json" \
    --remote_instance_name="projects/${KOKORO_PROJECT_ID}" \
    --invocation_id="${INVOCATION_ID}" \
    --config=results-local \
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
# Then build everything else (integration tests, examples, etc). This needs to
# go last (and in a separate invocation) so Bazel state is preserved to run the
# integration tests.
bazel build \
    --test_output=errors \
    --verbose_failures=true \
    --keep_going \
    -- //google/cloud/...:all

# The integration tests need further configuration and tools.
echo
echo "================================================================"
echo "================================================================"
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="$(bazel info output_base)/external/com_github_grpc_grpc/etc/roots.pem"
export GOOGLE_APPLICATION_CREDENTIALS="${KOKORO_GFILE_DIR}/service-account.json"

echo "Download dependencies for integration tests."
echo "    Getting cbt tool"
export GOPATH="${KOKORO_ROOT}/golang"
go get -u cloud.google.com/go/bigtable/cmd/cbt
# echo "    Getting python modules"
# sudo python2 -m pip install httpbin
echo "End of download."

echo ROOTS PATH = ${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}

ls -l ${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH} || echo "no roots file"
ls -l $(dirname ${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}) || echo "no roots file dir"

echo
echo "================================================================"
echo "================================================================"
echo "Running Google Cloud Bigtable Integration Tests"
(cd $(bazel info bazel-bin)/google/cloud/bigtable/tests && \
   "${PROJECT_ROOT}/google/cloud/bigtable/tests/run_integration_tests_production.sh")
(cd $(bazel info bazel-bin)/google/cloud/bigtable/tests && \
   "${PROJECT_ROOT}/google/cloud/bigtable/examples/run_examples_production.sh")

# TODO(#640) - disabled until we can get the credentials to work with key files.
# echo "Running Google Cloud Storage Integration Tests"
# (cd $(bazel info bazel-bin)/google/cloud/storage/tests && \
#     "${PROJECT_ROOT}/google/cloud/storage/tests/run_integration_tests_production.sh")
# echo "Running Google Cloud Storage Examples"
# (cd $(bazel info bazel-bin)/google/cloud/storage/examples && \
#     "${PROJECT_ROOT}/google/cloud/storage/examples/run_examples_production.sh")
