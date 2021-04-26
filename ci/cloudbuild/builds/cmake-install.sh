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
source module ci/cloudbuild/builds/lib/quickstart.sh
source module ci/lib/io.sh

export CC=clang
export CXX=clang++

INSTALL_PREFIX="$(mktemp -d)"
readonly INSTALL_PREFIX

# Compiles and installs all libraries and headers.
cmake -GNinja \
  -DBUILD_TESTING=OFF \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -S . -B cmake-out
cmake --build cmake-out
cmake --install cmake-out --component google_cloud_cpp_development

io::log_h2 "Verifying installed directories"
# Finds all the installed leaf directories (i.e., directories with exactly two
# links: . and ..). We only look at leaf directories, because obviously all
# parent directories must also exist.
mapfile -t actual_dirs < <(env -C "${INSTALL_PREFIX}" find -type d -links 2)
expected_dirs=(
  ./include/google/api
  ./include/google/bigtable/admin/v2
  ./include/google/bigtable/v2
  ./include/google/cloud/bigquery/connection/v1beta1
  ./include/google/cloud/bigquery/datatransfer/v1
  ./include/google/cloud/bigquery/internal
  ./include/google/cloud/bigquery/logging/v1
  ./include/google/cloud/bigquery/mocks
  ./include/google/cloud/bigquery/storage/v1
  ./include/google/cloud/bigquery/v2
  ./include/google/cloud/bigtable/internal
  ./include/google/cloud/dialogflow/v2
  ./include/google/cloud/dialogflow/v2beta1
  ./include/google/cloud/firestore
  ./include/google/cloud/grpc_utils
  ./include/google/cloud/iam/internal
  ./include/google/cloud/iam/mocks
  ./include/google/cloud/internal
  ./include/google/cloud/logging/internal
  ./include/google/cloud/logging/mocks
  ./include/google/cloud/pubsub/internal
  ./include/google/cloud/pubsub/mocks
  ./include/google/cloud/spanner/internal
  ./include/google/cloud/spanner/mocks
  ./include/google/cloud/speech/v1
  ./include/google/cloud/storage/internal
  ./include/google/cloud/storage/oauth2
  ./include/google/cloud/storage/testing
  ./include/google/cloud/texttospeech/v1
  ./include/google/devtools/cloudtrace/v2
  ./include/google/iam/credentials/v1
  ./include/google/iam/v1
  ./include/google/logging/type
  ./include/google/logging/v2
  ./include/google/longrunning
  ./include/google/monitoring/v3
  ./include/google/pubsub/v1
  ./include/google/rpc
  ./include/google/spanner/admin/database/v1
  ./include/google/spanner/admin/instance/v1
  ./include/google/spanner/v1
  ./include/google/storage/v1
  ./include/google/type
  ./lib64/cmake/bigtable_client
  ./lib64/cmake/firestore_client
  ./lib64/cmake/google_cloud_cpp_bigquery
  ./lib64/cmake/google_cloud_cpp_bigtable
  ./lib64/cmake/google_cloud_cpp_common
  ./lib64/cmake/google_cloud_cpp_firestore
  ./lib64/cmake/google_cloud_cpp_googleapis
  ./lib64/cmake/google_cloud_cpp_grpc_utils
  ./lib64/cmake/google_cloud_cpp_iam
  ./lib64/cmake/google_cloud_cpp_logging
  ./lib64/cmake/google_cloud_cpp_pubsub
  ./lib64/cmake/google_cloud_cpp_spanner
  ./lib64/cmake/google_cloud_cpp_storage
  ./lib64/cmake/googleapis
  ./lib64/cmake/pubsub_client
  ./lib64/cmake/spanner_client
  ./lib64/cmake/storage_client
  ./lib64/pkgconfig
)

# Assume a successful exit. Any test can set this to 1 to indicate failure.
exit_code=0

# Fails on any difference between the expected vs actually installed dirs.
discrepancies="$(comm -3 \
  <(printf "%s\n" "${expected_dirs[@]}" | sort) \
  <(printf "%s\n" "${actual_dirs[@]}" | sort))"
if [[ -n "${discrepancies}" ]]; then
  io::log "Found install discrepancies: expected vs actual"
  echo "${discrepancies}"
  exit_code=1
fi

io::log_h2 "Validating installed pkg-config files"
export PKG_CONFIG_PATH="${INSTALL_PREFIX}/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"
while IFS= read -r -d '' pc; do
  # Ignores the warning noise from system .pc files, but we redo the validation
  # with warnings enabled if the validation of our .pc fails.
  if ! pkg-config --validate "${pc}" >/dev/null 2>&1; then
    echo "Invalid ${pc}"
    pkg-config --validate "${pc}" || true
    echo "---"
    cat "${pc}"
    echo "---"
    exit_code=1
  fi
done < <(find "${INSTALL_PREFIX}" -name '*.pc' -print0)

io::log_h2 "Validating installed file extensions"
# All installed libraries have the same version, so pick one.
version=$(pkg-config google_cloud_cpp_common --modversion)
version_major=$(cut -d. -f1 <<<"${version}")
allow_re="\.(h|inc|proto|cmake|pc|a|so|so\.${version}|so\.${version_major})\$"
while IFS= read -r -d '' f; do
  if ! grep -qP "${allow_re}" <<<"${f}"; then
    echo "File with unexpected suffix installed: ${f}"
    exit_code=1
  fi
done < <(find "${INSTALL_PREFIX}" -type f -print0)

for repo_root in "ci/verify_current_targets" "ci/verify_deprecated_targets"; do
  out_dir="cmake-out/$(basename "${repo_root}")-out"
  io::log_h2 "Verifying CMake targets in repo root: ${repo_root}"
  cmake -GNinja -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
    -S "${repo_root}" -B "${out_dir}" -Wno-dev
  cmake --build "${out_dir}"
  cmake --build "${out_dir}" --target test
done

# Tests the installed artifacts by building and running the quickstarts.
quickstart::build_cmake_and_make "${INSTALL_PREFIX}"
quickstart::run_cmake_and_make "${INSTALL_PREFIX}"

# Deletes all the installed artifacts, and installs only the runtime components
# to verify that we can still execute the compiled quickstart programs.
rm -rf "${INSTALL_PREFIX:?}"/{include,lib64}
cmake --install cmake-out --component google_cloud_cpp_runtime
quickstart::run_cmake_and_make "${INSTALL_PREFIX}"

exit "${exit_code}"
