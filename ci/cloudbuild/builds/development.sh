#!/bin/bash
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
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/features.sh
source module ci/cloudbuild/builds/lib/integration.sh
source module ci/cloudbuild/builds/lib/cloudcxxrc.sh
source module ci/lib/io.sh

export CC=clang
export CXX=clang++
export CTCACHE_DIR=~/.cache/ctcache

# Always remove the CMakeCache because it may be invalidated by `cloudcxxrc`
# changes.
if [[ "${ALWAYS_RESET_CMAKE_CACHE}" = "YES" ]]; then
  io::run rm -f cmake-out/CMakeCache.txt
fi

# Note: we use C++14 for this build because we don't want tidy suggestions that
# require a newer C++ standard.
mapfile -t cmake_args < <(cmake::common_args)
io::run cmake "${cmake_args[@]}" \
  -DCMAKE_CXX_CLANG_TIDY=/usr/local/bin/clang-tidy-wrapper \
  -DGOOGLE_CLOUD_CPP_ENABLE_CLANG_ABI_COMPAT_17=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_STANDARD=14 \
  -DGOOGLE_CLOUD_CPP_ENABLE="${ENABLED_FEATURES}"
io::run cmake --build cmake-out

if [[ "${RUN_CLANG_TIDY_FIX}" = "YES" ]]; then
  io::bold clang-tidy -p cmake-out -fix
  git_files -z -- '*.h' '*.cc' |
    xargs -r -P "$(nproc)" -n 1 -0 clang-tidy -p cmake-out -fix
  io::run cmake --build cmake-out
fi

if [[ "${RUN_UNIT_TESTS}" = "YES" ]]; then
  mapfile -t ctest_args < <(ctest::common_args)
  io::run ctest --test-dir cmake-out "${ctest_args[@]}" -LE integration-test
fi

if [[ "${RUN_INTEGRATION_TESTS}" = "YES" ]]; then
  integration::ctest_with_emulators "cmake-out"
fi

# This build should fail if any of the above work generated code differences.
# This may be updated `.bzl` files, or files updated by clang-tidy.
io::log_h2 "Highlight generated code differences"
git diff --exit-code
