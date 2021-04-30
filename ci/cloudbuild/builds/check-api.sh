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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh

export CC=gcc
export CXX=g++

INSTALL_PREFIX=/var/tmp/google-cloud-cpp
# TODO(#6313): Compile with `-Og`, which abi-dumper wants.
cmake -GNinja \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=Debug -S . -B cmake-out
cmake --build cmake-out
cmake --build cmake-out --target install

# Uses `abi-dumper` to dump the ABI for the given library, which should be
# installed at the given prefix. This function will be called from a subshell,
# so it cannot use other variables or functions (including io::log*).
function dump_abi() {
  local library="$1"
  local prefix="$2"
  echo "Dumping ${library} (may be slow)..."
  abi-dumper "${prefix}/lib64/lib${library}.so" \
    -public-headers "${prefix}/include" \
    -lver "actual" \
    -o "cmake-out/${library}.actual.abi.dump"
}
export -f dump_abi # enables this function to be called from a subshell

libraries=(
  "google_cloud_cpp_bigtable"
  "google_cloud_cpp_common"
  "google_cloud_cpp_grpc_utils"
  "google_cloud_cpp_spanner"
  "google_cloud_cpp_storage"
  "google_cloud_cpp_pubsub"
)

# Run the dump_abi function for each library in parallel since its slow.
echo "${libraries[@]}" | xargs -P "$(nproc)" -n 1 \
  bash -c "dump_abi \$0 ${INSTALL_PREFIX}"

# A count of the number of libraries that fail the api compliance check.
# This will become the script's exit code.
errors=0

for lib in "${libraries[@]}"; do
  io::log_h2 "Checking ${lib}"
  actual_dump_file="${lib}.actual.abi.dump"
  expected_dump_file="${lib}.expected.abi.dump"
  expected_dump_path="${PROJECT_ROOT}/ci/test-abi/${expected_dump_file}.gz"
  zcat "${expected_dump_path}" >"cmake-out/${expected_dump_file}"
  # We ignore all symbols in internal namespaces, because these are not part
  # of our public API. We do this by specifying a regex that matches against
  # the mangled symbol names. For example, 8 is the number of characters in
  # the string "internal", and it should again be followed by some other
  # number indicating the length of the symbol within the "internal"
  # namespace. See: https://en.wikipedia.org/wiki/Name_mangling
  report="cmake-out/compat_reports/${lib}/expected_to_actual/src_compat_report.html"
  if ! abi-compliance-checker \
    -skip-internal-symbols "(8internal|15pubsub_internal|16spanner_internal)\d" \
    -report-path "${report}" \
    -src -l "${lib}" \
    -old "cmake-out/${expected_dump_file}" \
    -new "cmake-out/${actual_dump_file}"; then
    io::log_red "ABI Compliance error: ${lib}"
    ((++errors))
    io::log "Report file: ${report}"
    w3m -dump "${report}"
  fi
  # Replaces the (old) expected dump file with the (new) actual one.
  gzip "cmake-out/${actual_dump_file}"
  mv -f "cmake-out/${actual_dump_file}.gz" "${expected_dump_path}"
done
echo
exit "${errors}"
