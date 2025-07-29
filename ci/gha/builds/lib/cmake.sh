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

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_CMAKE_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

io::log_h1 "CMake Info"
printf "%10s %s\n" "cmake:" "$(cmake --version || echo not available)"
printf "%10s %s\n" "ninja:" "$(ninja --version || echo not available)"
printf "%10s %s\n" "sccache:" "$(sccache --version || echo not available)"

if command -v sccache >/dev/null 2>&1; then
  io::log "Clearing sccache stats"
  sccache --zero-stats
  function show_stats_handler() {
    io::log "Final sccache stats"
    sccache --show-stats
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
  if command -v sccache >/dev/null 2>&1; then
    args+=(
      -DCMAKE_CXX_COMPILER_LAUNCHER=sccache
    )
  fi
  if [[ -n "${VCPKG_TRIPLET:-}" ]]; then
    args+=("-DVCPKG_TARGET_TRIPLET=${VCPKG_TRIPLET}")
  fi
  args+=("-DVCPKG_OVERLAY_PORTS=../vcpkg-overlays")
  printf "%s\n" "${args[@]}"
}

function cmake::vcpkg_args() {
  local args
  args=(
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  )
  printf "%s\n" "${args[@]}"
}
