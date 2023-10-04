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

# This bash library has various helper functions for our cmake-based builds.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/cloudbuild/builds/lib/features.sh
source module ci/lib/io.sh

io::log "Using CMake version"
cmake --version

# Adds an elapsed seconds counter at the beginning of the ninja output to help
# us see where builds are taking the most time. See also
# https://ninja-build.org/manual.html#_environment_variables
export NINJA_STATUS="T+%es [%f/%t] "

# This block is run the first (and only) time this script is sourced. It first
# clears the sccache stats. Then it registers an exit handler that will display
# the ccache stats when the calling script exits.
if command -v sccache >/dev/null 2>&1; then
  io::log "Clearing sccache stats"
  sccache --zero-stats
  function show_stats_handler() {
    if [[ "${TRIGGER_TYPE:-}" != "manual" || "${VERBOSE_FLAG:-}" == "true" ]]; then
      io::log "===> sccache stats"
      sccache --show-stats
    fi
  }
  trap show_stats_handler EXIT
fi

function cmake::common_args() {
  local binary="cmake-out"
  if [[ $# -ge 1 ]]; then
    binary="$1"
  fi
  local args
  args=(
    -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=OFF
    -DGOOGLE_CLOUD_CPP_ENABLE_WERROR=ON
    -GNinja
    -S .
    -B "${binary}"
  )
  if command -v /usr/local/bin/sccache >/dev/null 2>&1; then
    args+=(
      -DCMAKE_CXX_COMPILER_LAUNCHER=/usr/local/bin/sccache
    )
  fi
  printf "%s\n" "${args[@]}"
}
