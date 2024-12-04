#!/bin/bash
#
# Copyright 2024 Google LLC
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

set -eo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/lib/io.sh
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/cloudcxxrc.sh
source module ci/cloudbuild/builds/lib/universe_domain.sh

export CC=clang
export CXX=clang++

if [[ -n "${UD_SA_KEY_FILE}" ]]; then
  ud::bazel_run //google/cloud/universe_domain/demo:kms_demo \
    "${UD_PROJECT}" "${UD_REGION}" "${UD_SA_KEY_FILE}"
  ud::bazel_test //google/cloud/storage/tests:universe_domain_integration_test
  ud::bazel_test //google/cloud/universe_domain/integration_tests:impersonation_tests
else
  source module ci/etc/integration-tests-config.sh
  bazel run -- //google/cloud/universe_domain/demo:kms_demo \
    "${GOOGLE_CLOUD_PROJECT}" "${GOOGLE_CLOUD_CPP_TEST_REGION}"
fi
