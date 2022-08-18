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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/integration.sh

export CC=clang
export CXX=clang++
export CTCACHE_DIR=~/.cache/ctcache
mapfile -t cmake_args < <(cmake::common_args)

# See https://github.com/matus-chochlik/ctcache for docs about the clang-tidy-cache
# Note: we use C++11 for this build because we don't want tidy suggestions that
# require a newer C++ standard.
cmake "${cmake_args[@]}" \
  -DCMAKE_CXX_CLANG_TIDY=/usr/local/bin/clang-tidy-wrapper \
  -DCMAKE_CXX_STANDARD=11 \
  -DGOOGLE_CLOUD_CPP_ENABLE="bigtable;bigquery;generator;iam;logging;pubsub;pubsublite;spanner;storage" \
  -DGOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC=ON
cmake --build cmake-out

mapfile -t ctest_args < <(ctest::common_args)
env -C cmake-out ctest "${ctest_args[@]}" -LE integration-test

integration::ctest_with_emulators "cmake-out"

if [[ "${TRIGGER_TYPE}" != "manual" ]]; then
  # This build should fail if any of the above work generated
  # code differences (for example, by updating .bzl files).
  io::log_h2 "Highlight generated code differences"
  git diff --exit-code
fi

if [[ "${TRIGGER_TYPE}" != "manual" || "${VERBOSE_FLAG}" == "true" ]]; then
  io::log_h2 "ctcache stats"
  printf "%s: %s\n" "total size" "$(du -sh "${CTCACHE_DIR}")"
  printf "%s: %s\n" " num files" "$(find "${CTCACHE_DIR}" | wc -l)"
  echo
fi
