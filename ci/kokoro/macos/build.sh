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

echo
echo "================================================================"
echo "================================================================"
echo "Define GOOGLE_APPLICATION_CREDENTIALS."
export GOOGLE_APPLICATION_CREDENTIALS="${KOKORO_GFILE_DIR}/service-account.json"

echo
echo "================================================================"
echo "================================================================"
echo "Define GRC_DEFAULT_SSL_ROOTS_FILE_PATH."
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="$PWD/roots.pem"

echo
echo "================================================================"
echo "================================================================"
echo "Create kokoro-bazelrc."
cat >>kokoro-bazelrc <<_EOF_
# Set flags for uploading to BES without Remote Build Execution.
startup --host_jvm_args=-Dbazel.DigestFunction=SHA256
build:results-local --remote_cache=remotebuildexecution.googleapis.com
build:results-local --spawn_strategy=local
build:results-local --remote_timeout=3600
build:results-local --bes_backend="buildeventservice.googleapis.com"
build:results-local --bes_timeout=10m
build:results-local --tls_enabled=true
build:results-local --auth_enabled=true
build:results-local --auth_scope=https://www.googleapis.com/auth/cloud-source-tools
_EOF_

echo
echo "================================================================"
echo "================================================================"
# First build and run the unit tests.
invocation_id="$(python -c 'import uuid; print uuid.uuid4()')"
echo "Configure and start Bazel: " ${invocation_id}
echo "================================================================"
echo "https://source.cloud.google.com/results/invocations/${invocation_id}"
echo "================================================================"
echo ${invocation_id} >> "${KOKORO_ARTIFACTS_DIR}/bazel_invocation_ids"

bazel --bazelrc=kokoro-bazelrc test \
    --copt=-DGRPC_BAZEL_BUILD \
    --action_env=GOOGLE_APPLICATION_CREDENTIALS="${GOOGLE_APPLICATION_CREDENTIALS}" \
    --test_output=errors \
    --verbose_failures=true \
    --keep_going \
    --project_id=${KOKORO_PROJECT_ID} \
    --auth_credentials="${KOKORO_GFILE_DIR}/build-results-service-account.json" \
    --remote_instance_name="projects/${KOKORO_PROJECT_ID}" \
    --invocation_id="${invocation_id}" \
    --config=results-local \
    -- //google/cloud/...:all

echo
echo "================================================================"
echo "================================================================"
bazel --bazelrc=kokoro-bazelrc build \
    --copt=-DGRPC_BAZEL_BUILD \
    --action_env=GOOGLE_APPLICATION_CREDENTIALS="${GOOGLE_APPLICATION_CREDENTIALS}" \
    --test_output=errors \
    --verbose_failures=true \
    --keep_going \
    --project_id=${KOKORO_PROJECT_ID} \
    --auth_credentials="${KOKORO_GFILE_DIR}/build-results-service-account.json" \
    --remote_instance_name="projects/${KOKORO_PROJECT_ID}" \
    --invocation_id="${invocation_id}" \
    --config=results-local \
    -- //google/cloud/...:all

