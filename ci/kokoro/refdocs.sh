#!/usr/bin/env bash

# Copyright 2019 Google LLC
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

set -euo pipefail

# For docuploader to work
export LC_ALL=C.UTF-8
export LANG=C.UTF-8

readonly BUILD_OUTPUT="cmake-out/refdocs"

upload_docs() {
  local docs_name="${1}"
  local docs_dir="${2}"
  local branch="${3}"
  if [[ ! -d "${docs_dir}" ]]; then
    echo "Document not found at: ${docs_dir}, skipping"
    return 0
  fi
  pushd "${docs_dir}"
  if [[ "${branch}" == "master" ]]; then
    docs_version="master"
  else
    # Extract the version from index.html
    docs_version=$(sed -n 's:.*<span id="projectnumber">\(.*\)</span>.*:\1:p' \
      index.html)
  fi

  # Create docs metadata
  python3 -m docuploader create-metadata \
    --name "${docs_name}" \
    --version "${docs_version}" \
    --language cpp

  # Upload the docs
  python3 -m docuploader upload . \
    --credentials "${CREDENTIALS_FILE}" \
    --staging-bucket "${STAGING_BUCKET}"
  popd
  return 0
}

if [[ -z "${CREDENTIALS_FILE:-}" ]]; then
  CREDENTIALS_FILE="${KOKORO_KEYSTORE_DIR}/73713_docuploader_service_account"
fi

if [[ -z "${STAGING_BUCKET:-}" ]]; then
  STAGING_BUCKET="docs-staging"
fi

# work from the git root directory
cd "$(dirname "$0")/../../"

# Try to determine the branch
if [[ -z "${KOKORO_GITHUB_COMMIT:-}" ]]; then
  echo "KOKORO_GITHUB_COMMIT not found, aborting..."
  exit 1
else
  branch=$(git branch --all --no-color --contains "${KOKORO_GITHUB_COMMIT}" \
    | grep -v HEAD | head -1)
  # trim branch
  shopt -s extglob
  branch="${branch##*( )}"
  branch="${branch%%*( )}"
  branch="${branch##remotes/origin/}"
  shopt -u extglob
  echo "branch detected: ${branch}"
fi

ADDITIONAL_CMAKE_FLAGS=""
cmake_flags=(
  # Compile with shared libraries
  "-DBUILD_SHARED_LIBS=ON"
  # To speed up the build
  "-DBUILD_TESTING=OFF"
  "-DCMAKE_BUILD_TYPE=Debug"
  "-DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package"
  # Use directory layout for googleapis.dev
  "-DGOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV=on"
)

if [[ "${branch}" == "master" ]]; then
  cmake_flags+=("-DGOOGLE_CLOUD_CPP_USE_MASTER_FOR_REFDOC_LINKS=on")
fi

# Build doxygen docs
cmake -H. "-B${BUILD_OUTPUT}" "${cmake_flags[@]}"
cmake --build "${BUILD_OUTPUT}" -- -j "$(nproc)"
cmake --build "${BUILD_OUTPUT}" --target install
cmake --build "${BUILD_OUTPUT}" --target doxygen-docs

upload_docs "google-cloud-common" "${BUILD_OUTPUT}/google/cloud/html" \
  "${branch}"
upload_docs "google-cloud-bigtable" \
  "${BUILD_OUTPUT}/google/cloud/bigtable/html" "${branch}"
upload_docs "google-cloud-storage" "${BUILD_OUTPUT}/google/cloud/storage/html" \
  "${branch}"
upload_docs "google-cloud-firestore" \
  "${BUILD_OUTPUT}/google/cloud/firestore/html" "${branch}"
