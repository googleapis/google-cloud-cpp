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

export CC=gcc
export CXX=g++

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/vcpkg.sh
source module ci/etc/quickstart-config.sh
source module ci/lib/io.sh

io::log_h2 "Installing google-cloud-cpp with vcpkg"
vcpkg_dir="$(vcpkg::root_dir)"
env -C "${vcpkg_dir}" ./vcpkg remove --outdated --recurse
env -C "${vcpkg_dir}" ./vcpkg install --recurse "google-cloud-cpp[*]"

# Compiles all the quickstart builds
mapfile -t features < <(
  env -C "${vcpkg_dir}" ./vcpkg search google-cloud-cpp |
    sed -n -e 's/^google-cloud-cpp\[\(.*\)\].*/\1/p' |
    sed -e 's/dialogflow-/dialogflow_/' \
      -e '/^grafeas$/d' \
      -e '/^experimental-/d' \
      -e '/^grpc-common$/d' \
      -e '/^rest-common$/d'
)
feature_list="$(printf ";%s" "${features[@]}")"
feature_list="${feature_list:1}"
io::run cmake -G Ninja \
  -S "${PROJECT_ROOT}/ci/verify_quickstart" \
  -B "${PROJECT_ROOT}/cmake-out/quickstart" \
  -DCMAKE_TOOLCHAIN_FILE="${vcpkg_dir}/scripts/buildsystems/vcpkg.cmake" \
  -DFEATURES="${feature_list}"
io::run cmake --build "${PROJECT_ROOT}/cmake-out/quickstart" --target verify-quickstart-cmake
