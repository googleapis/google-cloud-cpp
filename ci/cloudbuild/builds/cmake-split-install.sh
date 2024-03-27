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
source module ci/lib/io.sh

read -r ENABLED_FEATURES < <(features::list_full_cmake)
readonly ENABLED_FEATURES

# Compiles and installs the core libraries.
INSTALL_PREFIX="$(mktemp -d)"
readonly INSTALL_PREFIX

install_args=(
  # Avoid per-file install logs.
  -DCMAKE_INSTALL_MESSAGE=NEVER
  # Do not compile the tests or examples when testing install rules.
  -DBUILD_TESTING=OFF
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
  # Test with shared libraries because the problems are more obvious with them.
  -DBUILD_SHARED_LIBS=ON
)

io::log_h2 "Building and installing common libraries"
# We need a custom build directory
mapfile -t core_cmake_args < <(cmake::common_args cmake-out/common-libraries)
io::run cmake "${core_cmake_args[@]}" "${install_args[@]}" \
  -DGOOGLE_CLOUD_CPP_ENABLE="__common__"
io::run cmake --build cmake-out/common-libraries
io::run cmake --install cmake-out/common-libraries --prefix "${INSTALL_PREFIX}"

io::log_h2 "Building and installing popular libraries"
mapfile -t core_cmake_args < <(cmake::common_args cmake-out/popular-libraries)
io::run cmake "${core_cmake_args[@]}" "${install_args[@]}" \
  -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
  -DGOOGLE_CLOUD_CPP_ENABLE="bigtable,pubsub,spanner,storage,storage_grpc,.iam,policytroubleshooter" \
  -DGOOGLE_CLOUD_CPP_USE_INSTALLED_COMMON=ON
io::run cmake --build cmake-out/popular-libraries
io::run cmake --install cmake-out/popular-libraries --prefix "${INSTALL_PREFIX}"

io::log_h2 "Building and installing Compute"
# We need a custom build directory
mapfile -t feature_cmake_args < <(cmake::common_args cmake-out/compute)
io::run cmake "${feature_cmake_args[@]}" "${install_args[@]}" \
  -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
  -DGOOGLE_CLOUD_CPP_USE_INSTALLED_COMMON=ON \
  -DGOOGLE_CLOUD_CPP_ENABLE="compute"
io::run cmake --build cmake-out/compute
io::run cmake --install cmake-out/compute --prefix "${INSTALL_PREFIX}"

io::log_h2 "Building and installing all features"
# We need a custom build directory
mapfile -t feature_cmake_args < <(cmake::common_args cmake-out/features)
io::run cmake "${feature_cmake_args[@]}" "${install_args[@]}" \
  -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
  -DGOOGLE_CLOUD_CPP_USE_INSTALLED_COMMON=ON \
  -DGOOGLE_CLOUD_CPP_ENABLE="__ga_libraries__,-bigtable,-pubsub,-storage,-storage_grpc,-spanner,-iam,-policytroubleshooter,-compute"
io::run cmake --build cmake-out/features
io::run cmake --install cmake-out/features --prefix "${INSTALL_PREFIX}"

# Tests the installed artifacts by building all the quickstarts.
# shellcheck disable=SC2046
mapfile -t feature_list < <(cmake -P cmake/print-ga-features.cmake 2>&1 | grep -v storage_grpc)
FEATURES=$(printf ";%s" "${feature_list[@]}")
FEATURES="${FEATURES:1}"
io::run cmake -G Ninja \
  -S "${PROJECT_ROOT}/ci/verify_quickstart" \
  -B "${PROJECT_ROOT}/cmake-out/quickstart" \
  "-DCMAKE_PREFIX_PATH=${INSTALL_PREFIX}" \
  "-DFEATURES=${FEATURES}"
cmake --build "${PROJECT_ROOT}/cmake-out/quickstart"
