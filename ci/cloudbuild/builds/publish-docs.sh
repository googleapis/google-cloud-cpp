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

version=""
doc_args=(
  "-DCMAKE_BUILD_TYPE=Debug"
  "-DGOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV=ON"
  "-DDOXYGEN_CLANG_OPTIONS=-resource-dir=$(clang -print-resource-dir)"
)

# Extract the version number if we're on a release branch.
if grep -qP 'v\d+\.\d+\..*' <<<"${BRANCH_NAME}"; then
  version_file="google/cloud/internal/version_info.h"
  major="$(awk '/GOOGLE_CLOUD_CPP_VERSION_MAJOR/{print $3}' "${version_file}")"
  minor="$(awk '/GOOGLE_CLOUD_CPP_VERSION_MINOR/{print $3}' "${version_file}")"
  patch="$(awk '/GOOGLE_CLOUD_CPP_VERSION_PATCH/{print $3}' "${version_file}")"
  version="${major}.${minor}.${patch}"
else
  version="HEAD"
  doc_args+=("-DGOOGLE_CLOUD_CPP_USE_MASTER_FOR_REFDOC_LINKS=ON")
fi

# |---------------------------------------------|
# |      Bucket       |           URL           |
# |-------------------|-------------------------|
# |   docs-staging    |     googleapis.dev/     |
# | test-docs-staging | staging.googleapis.dev/ |
# |---------------------------------------------|
# For PR and manual builds, publish to the staging site. For CI builds (release
# and otherwise), publish to the public URL.
bucket="test-docs-staging"
if [[ "${TRIGGER_TYPE}" == "ci" ]]; then
  bucket="docs-staging"
fi

export CC=clang
export CXX=clang++
cmake -GNinja "${doc_args[@]}" -S . -B cmake-out
# Doxygen needs the proto-generated headers, but there are race conditions
# between CMake generating these files and doxygen scanning for them. We could
# fix this by avoiding parallelism with `-j 1`, or as we do here, we'll
# pre-generate all the proto headers, then call doxygen.
cmake --build cmake-out --target google-cloud-cpp-protos
cmake --build cmake-out --target doxygen-docs

io::log_h2 "Installing the docuploader package $(date)"
python3 -m pip install --user gcp-docuploader protobuf \
  --upgrade --no-warn-script-location

# For docuploader to work
export LC_ALL=C.UTF-8
export LANG=C.UTF-8

# Uses the doc uploader to upload docs to a GCS bucket. Requires the package
# name and the path to the doxygen's "html" folder.
function upload_docs() {
  local package="$1"
  local docs_dir="$2"
  if [[ ! -d "${docs_dir}" ]]; then
    io::log_red "Directory not found: ${docs_dir}, skipping"
    return 0
  fi

  io::log_h2 "Uploading docs: ${package}"
  io::log "docs_dir=${docs_dir}"

  env -C "${docs_dir}" python3 -m docuploader create-metadata \
    --name "${package}" \
    --version "${version}" \
    --language cpp
  env -C "${docs_dir}" python3 -m docuploader upload . --staging-bucket "${bucket}"
}

if [[ "${PROJECT_ID:-}" != "cloud-cpp-testing-resources" ]]; then
  io::log_h2 "Skipping upload of docs," \
    "which can only be done in GCB project 'cloud-cpp-testing-resources'"
  exit 0
fi

io::log_h2 "Publishing docs"
io::log "version: ${version}"
io::log "branch:  ${BRANCH_NAME}"
io::log "bucket:  gs://${bucket}"

upload_docs "google-cloud-common" "cmake-out/google/cloud/html"
upload_docs "google-cloud-bigtable" "cmake-out/google/cloud/bigtable/html"
upload_docs "google-cloud-pubsub" "cmake-out/google/cloud/pubsub/html"
upload_docs "google-cloud-spanner" "cmake-out/google/cloud/spanner/html"
upload_docs "google-cloud-storage" "cmake-out/google/cloud/storage/html"
upload_docs "google-cloud-firestore" "cmake-out/google/cloud/firestore/html"
