#!/bin/bash
#
# Copyright 2022 Google LLC
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
source module ci/lib/io.sh
source module ci/cloudbuild/builds/lib/features.sh

mapfile -t features < <(features::list_full)

check_pkgconfig_relative() {
  local binary_dir="${1}"
  # shellcheck disable=SC2016
  mapfile -t unmatched < <(find "${binary_dir}" -name '*.pc' -print0 | xargs -0 grep -L 'includedir=${prefix}/')
  if [[ ${#unmatched[@]} -ne 0 ]]; then
    io::log_red "Found ${#unmatched} pkg-config module files with incorrect paths: " "$(printf "%s\n" "${unmatched[@]}")"
    return 1
  fi
  # shellcheck disable=SC2016
  mapfile -t unmatched < <(find "${binary_dir}" -name '*.pc' -print0 | xargs -0 grep -L 'libdir=${exec_prefix}/')
  if [[ ${#unmatched[@]} -ne 0 ]]; then
    io::log_red "Found ${#unmatched} pkg-config module files with incorrect paths: " "$(printf "%s\n" "${unmatched[@]}")"
    return 1
  fi
}

check_pkgconfig_absolute() {
  local binary_dir="${1}"
  mapfile -t unmatched < <(find "${binary_dir}" -name '*.pc' -print0 | xargs -0 grep -L includedir=/test-only/include)
  if [[ ${#unmatched[@]} -ne 0 ]]; then
    io::log_red "Found ${#unmatched} pkg-config module files with incorrect paths: " "$(printf "%s\n" "${unmatched[@]}")"
    return 1
  fi
  mapfile -t unmatched < <(find "${binary_dir}" -name '*.pc' -print0 | xargs -0 grep -L libdir=/test-only/lib)
  if [[ ${#unmatched[@]} -ne 0 ]]; then
    io::log_red "Found ${#unmatched} pkg-config module files with incorrect paths: " "$(printf "%s\n" "${unmatched[@]}")"
    return 1
  fi
}

for feature in "${features[@]}"; do
  io::run cmake -S . -B cmake-out/test-only-"${feature}" \
    -DGOOGLE_CLOUD_CPP_ENABLE="${feature}"
  io::run check_pkgconfig_relative cmake-out/test-only-"${feature}"

  io::run cmake -S . -B cmake-out/test-only-"${feature}"-absolute-cmake-install \
    -DGOOGLE_CLOUD_CPP_ENABLE="${feature}" \
    -DCMAKE_INSTALL_INCLUDEDIR=/test-only/include \
    -DCMAKE_INSTALL_LIBDIR=/test-only/lib
  io::run check_pkgconfig_absolute cmake-out/test-only-"${feature}"-absolute-cmake-install
done
