#!/usr/bin/env bash
#
# Copyright 2023 Google LLC
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
source module ci/gha/builds/lib/linux.sh
source module ci/gha/builds/lib/cmake.sh
source module ci/gha/builds/lib/ctest.sh

mapfile -t args < <(cmake::common_args)
mapfile -t vcpkg_args < <(cmake::vcpkg_args)
mapfile -t ctest_args < <(ctest::common_args)

# This is a build to test External Accounts. This is a feature to use accounts
# from providers other than Google to access Google services. In this case we
# are using "GitHub Actions" as the provider.
# The External Accounts feature is sometimes known as Workload Identity
# Federation, and sometimes BYOID (Bring Your Own ID).
features=(
  # Enable the smallest set of libraries libraries that will compile gRPC and
  # REST-based authentication components and tests.
  storage
  iam
  bigtable
)
enable=$(printf ";%s" "${features[@]}")
enable=${enable:1}

io::log_h1 "Starting Build"
TIMEFORMAT="==> 🕑 CMake configuration done in %R seconds"
time {
  io::run cmake "${args[@]}" "${vcpkg_args[@]}" -DGOOGLE_CLOUD_CPP_ENABLE="${enable}"
}

TIMEFORMAT="==> 🕑 CMake build done in %R seconds"
time {
  # Compile only the integration test we need for this build
  io::run cmake --build cmake-out --target common_internal_external_account_integration_test
}
