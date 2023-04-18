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
source module ci/cloudbuild/builds/lib/features.sh

mapfile -t FEATURE_LIST < <(features::list_full)
read -r ENABLED_FEATURES < <(features::list_full_cmake)

version=""
doc_args=(
  "-DCMAKE_BUILD_TYPE=Debug"
  "-DGOOGLE_CLOUD_CPP_GENERATE_DOXYGEN=ON"
  "-DGOOGLE_CLOUD_CPP_INTERNAL_DOCFX=ON"
  "-DGOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV=ON"
  "-DGOOGLE_CLOUD_CPP_ENABLE=${ENABLED_FEATURES}"
  "-DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON"
  "-DGOOGLE_CLOUD_CPP_ENABLE_WERROR=ON"
  "-DGOOGLE_CLOUD_CPP_DOXYGEN_CLANG_OPTIONS=-resource-dir=$(clang -print-resource-dir)"
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
cmake --build cmake-out --target doxygen-docs all-docfx

if [[ "${PROJECT_ID:-}" != "cloud-cpp-testing-resources" ]]; then
  io::log_h2 "Skipping upload of docs," \
    "which can only be done in GCB project 'cloud-cpp-testing-resources'"
  exit 0
fi

# To protect against supply chain attacks we need to use `--require-hashes` in
# `pip install` commands.  Googlers may want to see b/242562806 for more
# details.
#
# Because we are currently using a SHA (as opposed to a released version) of
# the `gcp-docuploader` the process to update these hashes is somewhat manual.
# This worked for me (coryan@) circa 2022-09-08:
#
#    venv="$(mktemp -d)"
#    python3 -m venv "${venv}/requirements"
#    source "${venv}/requirements/bin/activate"
#    pip install git+https://github.com/googleapis/docuploader@993badb47be3bf548a4c1726658eadba4bafeaca
#    pip freeze | grep -v gcp-docuploader >ci/etc/docuploader-requirements.in
#    pip install --require-hashes -r ci/etc/pip-tooling-requirements.txt
#    pip-compile --generate-hashes ci/etc/docuploader-requirements.in
#
io::log_h2 "Installing the docuploader package"
python3 -m pip install --upgrade --user --quiet --disable-pip-version-check \
  --no-warn-script-location --require-hashes -r ci/etc/docuploader-requirements.txt
python3 -m pip install --upgrade --user --quiet --disable-pip-version-check \
  --no-warn-script-location --no-deps \
  "git+https://github.com/googleapis/docuploader@993badb47be3bf548a4c1726658eadba4bafeaca"
python3 -m pip list

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

  env -C "${docs_dir}" "${PROJECT_ROOT}/ci/retry-command.sh" 3 120 \
    python3 -m docuploader create-metadata \
    --name "${package}" \
    --version "${version}" \
    --language cpp
  env -C "${docs_dir}" "${PROJECT_ROOT}/ci/retry-command.sh" 3 120 \
    python3 -m docuploader upload . --staging-bucket "${bucket}"
}

io::log_h2 "Publishing docs"
io::log "version: ${version}"
io::log "branch:  ${BRANCH_NAME}"
io::log "bucket:  gs://${bucket}"

upload_docs "google-cloud-common" "cmake-out/google/cloud/html"
for feature in "${FEATURE_LIST[@]}"; do
  if [[ "${feature}" == "experimental-storage-grpc" ]]; then continue; fi
  if [[ "${feature}" == "grafeas" ]]; then continue; fi
  upload_docs "google-cloud-${feature}" "cmake-out/google/cloud/${feature}/html"
done
