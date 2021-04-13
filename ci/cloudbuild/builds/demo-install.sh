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
#
# This build script serves two main purposes:
#
# 1. It demonstrates to users how to install `google-cloud-cpp`. We will
#    extract part of this script into user-facing markdown documentation.
# 2. It verifies that the installed artifacts work by compiling and running the
#    quickstart programs against the installed artifacts.

set -eu

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/etc/quickstart-config.sh
source module ci/lib/io.sh

## [START packaging.md]

# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
export PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="${PREFIX}" -H. -Bcmake-out
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install

## [END packaging.md]

# Tells pkg-config where the artifacts are, so the Makefile builds work.
export PKG_CONFIG_PATH="${PREFIX}/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"

# Verify that the installed artifacts are usable by running the quickstarts.
for lib in $(quickstart::libraries); do
  mapfile -t run_args < <(quickstart::arguments "${lib}")
  io::log_h2 "Building quickstart: ${lib}"
  if [[ "${PROJECT_ID:-}" != "cloud-cpp-testing-resources" ]]; then
    run_args=() # Empties these args so we don't execute quickstarts below
    io::log_yellow "Not executing quickstarts," \
      "which can only run in GCB project 'cloud-cpp-testing-resources'"
  fi

  io::log "[ CMake ]"
  src_dir="${PROJECT_ROOT}/google/cloud/${lib}/quickstart"
  bin_dir="${PROJECT_ROOT}/cmake-out/quickstart-${lib}"
  cmake -H"${src_dir}" -B"${bin_dir}" "-DCMAKE_PREFIX_PATH=${PREFIX}"
  cmake --build "${bin_dir}"
  test "${#run_args[@]}" -eq 0 || "${bin_dir}/quickstart" "${run_args[@]}"

  echo
  io::log "[ Make ]"
  make -C "${src_dir}"
  test "${#run_args[@]}" -eq 0 || "${src_dir}/quickstart" "${run_args[@]}"
done
