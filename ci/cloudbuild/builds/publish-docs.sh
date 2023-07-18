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
source module ci/lib/io.sh

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
docfx_bucket="docs-staging-v2-dev"
if [[ "${TRIGGER_TYPE}" == "ci" ]]; then
  bucket="docs-staging"
  docfx_bucket="docs-staging-v2"
fi

export CC=clang
export CXX=clang++
io::run cmake -GNinja "${doc_args[@]}" -S . -B cmake-out
# Doxygen needs the proto-generated headers, but there are race conditions
# between CMake generating these files and doxygen scanning for them. We could
# fix this by avoiding parallelism with `-j 1`, or as we do here, we'll
# pre-generate all the proto headers, then call doxygen.
io::run cmake --build cmake-out --target google-cloud-cpp-protos
io::run cmake --build cmake-out --target doxygen-docs all-docfx

if [[ "${PROJECT_ID:-}" != "cloud-cpp-testing-resources" ]]; then
  io::log_h2 "Skipping upload of docs," \
    "which can only be done in GCB project 'cloud-cpp-testing-resources'"
  exit 0
fi

# To protect against supply chain attacks we need to use `--require-hashes` in
# `pip install` commands.  Googlers may want to see b/242562806 for more
# details.
#
#    venv="$(mktemp -d)"
#    python3 -m venv "${venv}/requirements"
#    source "${venv}/requirements/bin/activate"
#    pip install gcp-docuploader
#    pip freeze >ci/etc/docuploader-requirements.in
#    pip install --require-hashes -r ci/etc/pip-tooling-requirements.txt
#    pip-compile --generate-hashes ci/etc/docuploader-requirements.in
#
io::log_h2 "Installing the docuploader package"
python3 -m venv "${HOME}/.venv/docuploader"
source "${HOME}/.venv/docuploader/bin/activate"
python3 -m pip install --upgrade --quiet --disable-pip-version-check \
  --no-warn-script-location --require-hashes -r ci/etc/docuploader-requirements.txt

# For docuploader to work
export LC_ALL=C.UTF-8
export LANG=C.UTF-8

# Stage documentation in DocFX format for processing. go/cloud-rad for details.
function stage_docfx() {
  local package="$1"
  local docfx_dir="$2"

  if [[ ! -d "${docfx_dir}" ]]; then
    io::log_red "Directory not found: ${docfx_dir}, skipping"
    return 0
  fi

  io::log "docfx_dir=${docfx_dir}"

  env -C "${docfx_dir}" "${PROJECT_ROOT}/ci/retry-command.sh" 3 120 \
    python3 -m docuploader upload \
    --staging-bucket "${docfx_bucket}" \
    --destination-prefix docfx .
}

exit_status=0

io::log_h2 "Publishing DocFX"
io::log "branch:  ${BRANCH_NAME}"
io::log "bucket:  gs://${docfx_bucket}"

for feature in common "${FEATURE_LIST[@]}"; do
  if [[ "${feature}" == "experimental-storage-grpc" ]]; then continue; fi
  if [[ "${feature}" == "grafeas" ]]; then continue; fi
  path="cmake-out/google/cloud/${feature}/docfx"
  if [[ "${feature}" == "common" ]]; then path="cmake-out/google/cloud/docfx"; fi
  io::log_h2 "Uploading docfx docs: ${feature}"
  # These uploads are extremely noisy. Just print their output on error.
  if ! mapfile -t LOG < <(stage_docfx "google-cloud-${feature}" "${path}" 2>&1); then
    for line in "${LOG[@]}"; do echo "${line}"; done
    exit_status=1
  fi
done

exit ${exit_status}
