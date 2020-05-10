#!/usr/bin/env bash
#
# Copyright 2018 Google LLC
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
source module lib/io.sh

if [[ $# != 1 ]]; then
  echo "Usage: $(basename "$0") <binary-directory>"
  exit 1
fi

readonly BINARY_DIR="$1"

if [[ "${CHECK_ABI:-}" != "yes" ]]; then
  echo
  io::log_yellow "Skipping ABI check as it is disabled for this build."
  exit 0
fi

check_library() {
  local library=$1
  local return_status=0

  echo
  io::log_yellow "Checking ABI for ${library} library."
  libdir="$(pkg-config "${library}" --variable=libdir)"
  includedir="$(pkg-config "${library}" --variable=includedir)"
  new_dump_file="${library}.actual.abi.dump"
  old_dump_file="${library}.expected.abi.dump"
  reference_file="${PROJECT_ROOT}/ci/test-abi/${old_dump_file}.gz"
  abi-dumper "${libdir}/lib${library}.so" \
    -public-headers "${includedir}" \
    -lver "current" \
    -o "${BINARY_DIR}/${new_dump_file}"

  # We want to collect the data for as many libraries as possible, do not exit
  # on the first error.
  set +e

  (
    cd "${BINARY_DIR}"
    zcat "${reference_file}" >"${old_dump_file}"
    abi-compliance-checker \
      -src -l "${library}" -old "${old_dump_file}" -new "${new_dump_file}"
  )
  if [[ $? != 0 ]]; then
    return_status=1
  fi
  set -e

  if [[ "${UPDATE_ABI}" == "yes" ]]; then
    abi-dumper "${libdir}/lib${library}.so" \
      -public-headers "${includedir}" \
      -lver "reference" -o "${BINARY_DIR}/${new_dump_file}"
    gzip -c "${BINARY_DIR}/${new_dump_file}" >"${reference_file}"
  fi
  return ${return_status}
}

exit_status=0
# We are keeping the library list alphabetical for now, there is no preferred
# order otherwise.
libraries=(
  "bigtable_client"
  "google_cloud_cpp_common"
  "google_cloud_cpp_grpc_utils"
  "spanner_client"
  "storage_client"
)
for library in "${libraries[@]}"; do
  check_library "${library}" || exit_status=1
done

exit ${exit_status}
