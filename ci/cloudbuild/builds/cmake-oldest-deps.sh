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

set -eu

export CC=clang
export CXX=clang++

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/integration.sh
source module ci/cloudbuild/builds/lib/vcpkg.sh
source module ci/lib/io.sh

io::log_h2 "Configuring"
vcpkg_root="$(vcpkg::root_dir)"
cmake -GNinja -S . -B cmake-out/build \
  "-DCMAKE_TOOLCHAIN_FILE=${vcpkg_root}/scripts/buildsystems/vcpkg.cmake" \
  "-DVCPKG_MANIFEST_DIR=ci/etc/oldest-deps" \
  "-DVCPKG_FEATURE_FLAGS=versions,manifest"

io::log_h2 "Building"
cmake --build cmake-out/build

io::log_h2 "Testing"
env -C cmake-out/build ctest -LE "integration-test" --parallel "$(nproc)"

integration::ctest_with_emulators "cmake-out/build"
