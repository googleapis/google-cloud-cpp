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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/quickstart.sh
source module ci/cloudbuild/builds/lib/features.sh
source module ci/lib/io.sh

export CC=clang
export CXX=clang++

INSTALL_PREFIX="$(mktemp -d)"
readonly INSTALL_PREFIX

read -r ENABLED_FEATURES < <(features::list_full_cmake)
mapfile -t cmake_args < <(cmake::common_args)

# Compiles and installs all libraries and headers.
cmake "${cmake_args[@]}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE="${ENABLED_FEATURES}"
cmake --build cmake-out
cmake --install cmake-out --component google_cloud_cpp_development

io::log_h2 "Verifying installed directories"
# Finds all the installed leaf directories (i.e., directories with exactly two
# links: . and ..). We only look at leaf directories, because obviously all
# parent directories must also exist.
mapfile -t actual_dirs < <(env -C "${INSTALL_PREFIX}" find -type d)
# The micro generator populates `ci/etc/expected_install_directories` with the
# expected directories generated via `make install`. This generated list does
# not include:
#   - hand-crafted libraries
#   - directories with pure protos (without gRPC code)
# We maintain those here as a hard-coded list.
mapfile -t expected_dirs < <(cat ci/etc/expected_install_directories)
expected_dirs+=(
  # no RPC services in google/cloud/appengine/legacy
  ./include/google/appengine/legacy
  # no RPC services in google/cloud/appengine/logging
  ./include/google/appengine/logging
  ./include/google/appengine/logging/v1
  ./include/google/cloud/bigquery/logging
  ./include/google/cloud/bigquery/logging/v1
  ./include/google/cloud/bigtable/admin/mocks
  # no RPC services in google/cloud/clouddms
  ./include/google/cloud/clouddms/logging
  ./include/google/cloud/clouddms/logging/v1
  # no RPC services in google/cloud/common
  ./include/google/cloud/common
  ./include/google/cloud/gkehub/v1/configmanagement
  ./include/google/cloud/gkehub/v1/multiclusteringress
  ./include/google/cloud/grpc_utils
  ./include/google/cloud/internal
  # orgpolicy/v1 is not automatically added. It is used by
  # google/cloud/asset, while google/cloud/orgpolicy uses
  # the **v2** protos.
  ./include/google/cloud/orgpolicy/v1
  # no RPC services in google/cloud/oslogin/common
  ./include/google/cloud/oslogin/common
  ./include/google/cloud/pubsub
  ./include/google/cloud/pubsub/internal
  # no gRPC services in google/cloud/recommender/logging
  ./include/google/cloud/recommender/logging
  ./include/google/cloud/recommender/logging/v1
  # no gRPC services in google/cloud/secretmanager/logging
  ./include/google/cloud/secretmanager/logging
  ./include/google/cloud/secretmanager/logging/v1
  ./include/google/cloud/pubsub/mocks
  ./include/google/cloud/spanner/admin/mocks
  ./include/google/cloud/spanner/internal
  ./include/google/cloud/spanner/mocks
  ./include/google/cloud/storage/oauth2
  ./include/google/cloud/storage/testing
  # no gRPC services in google/cloud/workflows/type.
  ./include/google/cloud/workflows/type
  ./include/google/devtools/source
  ./include/google/devtools/source/v1
  ./include/google/iam/v1
  # no gRPC services in google/identity/accesscontextmanager/type
  ./include/google/identity/accesscontextmanager/type
  ./include/google/logging/type
  ./include/google/longrunning
  ./include/google/pubsub
  ./include/google/pubsub/v1
  ./include/google/rpc
  ./include/google/spanner/v1
  ./include/google/type
  ./include/grafeas
  ./include/grafeas/v1
  ./lib64/cmake/google_cloud_cpp_common
  ./lib64/cmake/google_cloud_cpp_googleapis
  ./lib64/cmake/google_cloud_cpp_grafeas
  ./lib64/cmake/google_cloud_cpp_grpc_utils
  ./lib64/cmake/google_cloud_cpp_pubsub
  ./lib64/cmake/google_cloud_cpp_rest_internal
  ./lib64/cmake/google_cloud_cpp_spanner
  ./lib64/pkgconfig
)

# Fails on any difference between the expected vs actually installed dirs.
discrepancies="$(comm -3 \
  <(printf "%s\n" "${expected_dirs[@]}" | sort) \
  <(printf "%s\n" "${actual_dirs[@]}" | sort))"
if [[ -n "${discrepancies}" ]]; then
  io::log "Found install discrepancies: expected vs actual"
  echo "${discrepancies}"
  exit 1
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
    exit 1
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
    exit 1
  fi
done < <(find "${INSTALL_PREFIX}" -type f -print0)

mapfile -t ctest_args < <(ctest::common_args)
out_dir="cmake-out/verify_current_targets-out"
rm -f "${out_dir}/CMakeCache.txt"
io::log_h2 "Verifying CMake targets in repo root: ci/verify_current_targets"
cmake --log-level WARNING -GNinja -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
  -S ci/verify_current_targets -B "${out_dir}" -Wno-dev
cmake --build "${out_dir}"
env -C "${out_dir}" ctest "${ctest_args[@]}"

# Tests the installed artifacts by building and running the quickstarts.
# shellcheck disable=SC2046
libraries="$(printf ";%s" $(features::list_full | grep -v experimental- | grep -v grafeas))"
libraries="${libraries:1}"
cmake -G Ninja \
  -S "${PROJECT_ROOT}/ci/verify_quickstart" \
  -B "${PROJECT_ROOT}/cmake-out/quickstart" \
  "-DCMAKE_PREFIX_PATH=${INSTALL_PREFIX}" \
  "-DLIBRARIES=${libraries}"
cmake --build "${PROJECT_ROOT}/cmake-out/quickstart"

# Deletes all the installed artifacts, and installs only the runtime components
# to verify that we can still execute the compiled quickstart programs.
rm -rf "${INSTALL_PREFIX:?}"/{include,lib64}
cmake --install cmake-out --component google_cloud_cpp_runtime
quickstart::run_cmake_and_make "${INSTALL_PREFIX}"

# Be a little more explicit because we often run this manually
io::log_h1 "SUCCESS"
