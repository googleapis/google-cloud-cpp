#!/bin/bash
# shellcheck disable=SC2317
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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/lib/io.sh

export CC=gcc
export CXX=g++

mapfile -t cmake_args < <(cmake::common_args)
readonly INSTALL_PREFIX=/var/tmp/google-cloud-cpp

if [ "${GOOGLE_CLOUD_CPP_CHECK_API:-}" ]; then
  readonly ENABLED_FEATURES="${GOOGLE_CLOUD_CPP_CHECK_API}"
  IFS=',' read -ra library_list <<<"${GOOGLE_CLOUD_CPP_CHECK_API}"
else
  readonly ENABLED_FEATURES="__ga_libraries__,opentelemetry"
  mapfile -t library_list < <(cmake -P cmake/print-ga-libraries.cmake 2>&1)
  # These libraries are not "features", but they are part of the public API
  library_list+=("common" "grpc_utils")
  # This is a GA library, not included in __ga_libraries__
  library_list+=("opentelemetry")
fi

# abi-dumper wants us to use -Og, but that causes bogus warnings about
# uninitialized values with GCC, so we disable that warning with
# -Wno-maybe-uninitialized. See also:
# https://github.com/googleapis/google-cloud-cpp/issues/6313
io::run cmake "${cmake_args[@]}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGOOGLE_CLOUD_CPP_ENABLE="${ENABLED_FEATURES}" \
  -DCMAKE_CXX_FLAGS="-Og -Wno-maybe-uninitialized"
io::run cmake --build cmake-out
io::run cmake --install cmake-out >/dev/null

# Uses `abi-dumper` to dump the ABI for the given library, which should
# be installed at the given @p prefix, and `abi-compliance-checker` to
# produce a report comparing the old and new dumps. The (compressed) new
# dump is left in the @p project_root tree.
#
# This function will be called from a subshell, so it cannot use other
# variables or functions (including io::log*).
function check_abi() {
  local library="$1"
  local prefix="$2"
  local project_root="$3"

  local shortlib="${library#google_cloud_cpp_}"
  local public_headers="${prefix}/include/google/cloud/${shortlib}"
  if [[ "${shortlib}" == "common" || "${shortlib}" == "grpc_utils" || "${shortlib}" == "oauth2" ]]; then
    # These are special and share their header location.
    public_headers="${prefix}/include/google/cloud"
  elif [[ "${shortlib}" == "storage_grpc" ]]; then
    # `storage_grpc` uses the same header location as `storage`
    public_headers="${prefix}/include/google/cloud/storage"
  elif [[ "${shortlib}" =~ "compute" ]]; then
    # Compute libs are also special as their headers are in subdirectories.
    local computelib="${library#google_cloud_cpp_compute_}"
    public_headers="${prefix}/include/google/cloud/compute/${computelib}"
  fi

  local version
  version=$(git rev-parse --short HEAD)
  local actual_dump_file="${library}.actual.abi.dump"
  local -a dump_options=(
    # The source .so file
    "${prefix}/lib64/lib${library}.so"
    # Use the git version as the library version number for reporting purposes
    -lver "${version}"
    # The dump destination
    -o "cmake-out/${actual_dump_file}"
    # Where to find the headers
    -include-paths "${prefix}/include"
    -include-paths "/usr/local/include"
    # Where to find additional libraries
    -ld-library-path "${prefix}/lib64/:/usr/local/lib:/usr/local/lib64"
    # Treat all headers as public, we exclude internal symbols later
    -public-headers "${public_headers}"
    # Dump information about all symbols and types
    -all-symbols -all-types
    # Skip stdc++ and gnu c++ symbols
    -skip-cxx
    # Use the system's debuginfo
    -search-debuginfo /usr
  )
  abi-dumper "${dump_options[@]}" >/dev/null 2>&1 |
    grep -v "ERROR: missed type id" || true

  local project_dir="${project_root}/ci/abi-dumps"
  local expected_dump_file="${library}.expected.abi.dump"
  local expected_dump_path="${project_dir}/${expected_dump_file}.gz"
  if [[ -r "${expected_dump_path}" ]]; then
    zcat "${expected_dump_path}" >"cmake-out/${expected_dump_file}"
    report="cmake-out/compat_reports/${library}/src_compat_report.html"
    compliance_flags=(
      # Put the output report in a separate directory for each library
      -report-path "${report}"
      # We only want a source-level report. We make no ABI guarantees, such
      # as data structure sizes or virtual table ordering
      -src
      # We ignore all symbols in internal namespaces, because these are not
      # part of our public API. We do this by specifying a regex that matches
      # against the mangled symbol names. For example, 8 is the number of
      # characters in the string "internal", and it should again be followed
      # by some other number indicating the length of the symbol within the
      # "internal" namespace. See: https://en.wikipedia.org/wiki/Name_mangling
      -skip-internal-symbols "(8internal|_internal|4absl|4grpc|6google8protobuf|6google3rpc)\d"
      # We ignore the raw gRPC Stub class. The generated gRPC headers that
      # contain these classes are installed alongside our headers. When a new
      # RPC is added to a service, these classes gain a pure virtual method. Our
      # customers do not use these classes directly, so this should not
      # constitute a breaking change.
      #
      # Skip `Options::Data<>` which is a private implementation detail.
      # Otherwise, we get false positives when a specific type template
      # parameter is no longer used.
      -skip-internal-types "(::StubInterface|::Options::Data<)"
      # The library to compare
      -l "${library}"
      # Compared the saved baseline vs. the dump for the current version
      -old "cmake-out/${expected_dump_file}"
      -new "cmake-out/${actual_dump_file}"
    )
    abi-compliance-checker "${compliance_flags[@]}" >/dev/null || true
  fi

  # Replaces the (old) expected dump file with the (new) actual one.
  gzip -n "cmake-out/${actual_dump_file}"
  mv -f "cmake-out/${actual_dump_file}.gz" "${expected_dump_path}"
}
export -f check_abi # enables this function to be called from a subshell

mapfile -t libraries < <(printf "google_cloud_cpp_%s\n" "${library_list[@]}")

# Run the check_abi function for each library in parallel since it is slow.
echo "${libraries[@]}" | xargs -P "$(nproc)" -n 1 \
  bash -c "TIMEFORMAT=\"\${0#google_cloud_cpp_} completed in %0lR\";
           time check_abi \${0} ${INSTALL_PREFIX} ${PROJECT_ROOT}"

# A count of the number of libraries that fail the api compliance check.
# This will become the script's exit code.
errors=0

for library in "${libraries[@]}"; do
  report="cmake-out/compat_reports/${library}/src_compat_report.html"
  if grep --silent "<td class='compatible'>100%</td>" "${report}"; then
    io::log_green "ABI Compliance OK: ${library}"
  else
    io::log_red "ABI Compliance error: ${library}"
    io::log "Report file: ${report}"
    w3m -dump "${report}"
    ((++errors))
  fi
done

echo
exit "${errors}"
