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

export CC=clang
export CXX=clang++

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/ctest.sh
source module ci/cloudbuild/builds/lib/features.sh
source module ci/cloudbuild/builds/lib/integration.sh
source module ci/cloudbuild/builds/lib/vcpkg.sh
source module ci/lib/io.sh

# TODO(#14944): add bigtable back when we are able to update vcpkg version.
mapfile -t feature_list < <(features::always_build | grep -v "experimental-bigquery" | grep -v "bigtable")
ENABLED_FEATURES="$(printf ",%s" "${feature_list[@]}")"
ENABLED_FEATURES="${ENABLED_FEATURES:1}"
readonly ENABLED_FEATURES

io::log_h2 "Configuring"
vcpkg_root="$(vcpkg::root_dir)"
cmake -GNinja -S . -B cmake-out/build \
  "-DCMAKE_TOOLCHAIN_FILE=${vcpkg_root}/scripts/buildsystems/vcpkg.cmake" \
  "-DVCPKG_MANIFEST_DIR=ci/etc/oldest-deps" \
  "-DVCPKG_FEATURE_FLAGS=versions,manifest" \
  "-DGOOGLE_CLOUD_CPP_ENABLE=${ENABLED_FEATURES}"

io::log_h2 "Building"
cmake --build cmake-out/build

io::log_h2 "Testing"

mapfile -t ctest_args < <(ctest::common_args)
env -C cmake-out/build ctest "${ctest_args[@]}" -LE "integration-test"

integration::ctest_with_emulators "cmake-out/build"
