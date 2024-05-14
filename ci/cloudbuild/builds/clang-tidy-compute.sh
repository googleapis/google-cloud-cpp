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
source module ci/lib/io.sh

export CC=clang
export CXX=clang++
export CTCACHE_DIR=~/.cache/ctcache

mapfile -t cmake_args < <(cmake::common_args)
readonly ENABLED_FEATURES="compute"

# See https://github.com/matus-chochlik/ctcache for docs about the clang-tidy-cache
# Note: we use C++14 for this build because we don't want tidy suggestions that
# require a newer C++ standard.
io::run cmake "${cmake_args[@]}" \
  -DCMAKE_CXX_CLANG_TIDY=/usr/local/bin/clang-tidy-wrapper \
  -DGOOGLE_CLOUD_CPP_ENABLE_CLANG_ABI_COMPAT_17=ON \
  -DCMAKE_CXX_STANDARD=14 \
  -DGOOGLE_CLOUD_CPP_ENABLE="${ENABLED_FEATURES}" \
  -DGOOGLE_CLOUD_CPP_INTERNAL_DOCFX=ON
io::run cmake --build cmake-out

if [[ "${TRIGGER_TYPE}" != "manual" || "${VERBOSE_FLAG}" == "true" ]]; then
  io::log "===> ctcache stats"
  printf "%s: %s\n" "total size" "$(du -sh "${CTCACHE_DIR}")"
  printf "%s: %s\n" " num files" "$(find "${CTCACHE_DIR}" | wc -l)"
  echo
fi
