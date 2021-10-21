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

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_QUICKSTART_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_QUICKSTART_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_QUICKSTART_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/etc/quickstart-config.sh
source module ci/lib/io.sh

# Builds the CMake and Makefile quickstart programs but DOES NOT RUN THEM. This
# is a useful way to test that the artifacts installed by `google-cloud-cpp`
# work for compilation. This function requires a single argument specifying the
# directory where the `google-cloud-cpp` library was installed.
#
# Example:
#
#   quickstart::build_cmake_and_make "/usr/local"
function quickstart::build_cmake_and_make() {
  local prefix="$1"
  for lib in $(quickstart::libraries); do
    io::log_h2 "Building quickstart: ${lib}"
    local src_dir="${PROJECT_ROOT}/google/cloud/${lib}/quickstart"
    quickstart::build_one_quickstart "${prefix}" "${src_dir}" "${lib}"
  done
}

function quickstart::build_gcs_grpc_quickstart() {
  local prefix="$1"
  io::log_h2 "Building quickstart for GCS+gRPC"
  quickstart::build_one_quickstart "${prefix}" "google/cloud/storage/quickstart/grpc" "gcs-grpc"
}

function quickstart::build_one_quickstart() {
  local prefix="$1"
  local src_dir="$2"
  local bin_dir_suffix="$3"

  io::log "[ CMake ]"
  local cmake_bin_dir="${PROJECT_ROOT}/cmake-out/quickstart-cmake-${bin_dir_suffix}"
  cmake -H"${src_dir}" -B"${cmake_bin_dir}" "-DCMAKE_PREFIX_PATH=${prefix}"
  cmake --build "${cmake_bin_dir}"

  echo
  io::log "[ Make ]"
  local makefile_bin_dir="${PROJECT_ROOT}/cmake-out/quickstart-makefile-${bin_dir_suffix}"
  mkdir -p "${makefile_bin_dir}"
  PKG_CONFIG_PATH="${prefix}/lib64/pkgconfig:${prefix}/lib/pkgconfig:${PKG_CONFIG_PATH:-}" \
    make -C "${src_dir}" BIN="${makefile_bin_dir}"
}

# Runs the CMake and Makefile quickstart programs but DOES NOT COMPILE THEM.
# This function relies on the binaries already being compiled by a previous
# call to `quickstart::build_cmake_and_make`. This function requires a single
# argument specifying the directory where the `google-cloud-cpp` library was
# installed.
#
# Example:
#
#   quickstart::build_cmake_and_make "/usr/local"
#   quickstart::run_cmake_and_make "/usr/local"
function quickstart::run_cmake_and_make() {
  local prefix="$1"
  for lib in $(quickstart::libraries); do
    io::log_h2 "Running quickstart: ${lib}"
    mapfile -t run_args < <(quickstart::arguments "${lib}")
    quickstart::run_one_quickstart "${prefix}" "${lib}" "${run_args[@]}"
  done
}

function quickstart::run_gcs_grpc_quickstart() {
  local prefix="$1"
  mapfile -t run_args < <(quickstart::arguments "storage")
  io::log_h2 "Running quickstart for GCS+gRPC"
  quickstart::run_one_quickstart "${prefix}" "gcs-grpc" "${run_args[@]}"
}

function quickstart::run_one_quickstart() {
  local prefix="$1"
  local bin_dir_suffix="$2"
  shift 2
  local -a run_args=("$@")

  io::log "[ CMake ]"
  local cmake_bin_dir="${PROJECT_ROOT}/cmake-out/quickstart-cmake-${bin_dir_suffix}"
  "${cmake_bin_dir}/quickstart" "${run_args[@]}"

  echo
  io::log "[ Make ]"
  local makefile_bin_dir="${PROJECT_ROOT}/cmake-out/quickstart-makefile-${bin_dir_suffix}"
  LD_LIBRARY_PATH="${prefix}/lib64:${prefix}/lib:${LD_LIBRARY_PATH:-}" \
    "${makefile_bin_dir}/quickstart" "${run_args[@]}"
}
