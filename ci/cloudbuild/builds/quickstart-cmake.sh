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
env -C "${vcpkg_dir}" ./vcpkg install google-cloud-cpp

# Compiles and runs all the quickstart CMake builds.
for lib in $(quickstart::libraries); do
  io::log_h2 "Running CMake quickstart for ${lib}"
  src_dir="${PROJECT_ROOT}/google/cloud/${lib}/quickstart"
  bin_dir="${PROJECT_ROOT}/cmake-out/quickstart-${lib}"
  flag="-DCMAKE_TOOLCHAIN_FILE=${vcpkg_dir}/scripts/buildsystems/vcpkg.cmake"
  cmake -H"${src_dir}" -B"${bin_dir}" "${flag}"
  cmake --build "${bin_dir}"
  mapfile -t run_args < <(quickstart::arguments "${lib}")
  "${bin_dir}/quickstart" "${run_args[@]}"
done
