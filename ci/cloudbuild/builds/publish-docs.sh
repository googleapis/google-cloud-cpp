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
source module ci/cloudbuild/builds/lib/features.sh
source module ci/lib/io.sh

if [[ "${TRIGGER_TYPE}" == "manual" && "${LIBRARIES}" == "all" ]]; then
  LIBRARIES="all_bar_compute"
fi

if [[ "${LIBRARIES}" == "all" ]]; then
  mapfile -t FEATURE_LIST < <(features::list_full)
  read -r ENABLED_FEATURES < <(features::list_full_cmake)
elif [[ "${LIBRARIES}" == "all_bar_compute" ]]; then
  mapfile -t FEATURE_LIST < <(features::list_full | grep -v "^compute$")
  read -r ENABLED_FEATURES < <(features::list_full_cmake)
  ENABLED_FEATURES="${ENABLED_FEATURES},-compute"
else
  mapfile -t FEATURE_LIST < <(printf '%s' "${LIBRARIES}")
  ENABLED_FEATURES="compute"
fi

doc_args=(
  "-DCMAKE_BUILD_TYPE=Debug"
  "-DGOOGLE_CLOUD_CPP_GENERATE_DOXYGEN=ON"
  "-DGOOGLE_CLOUD_CPP_INTERNAL_DOCFX=ON"
  "-DGOOGLE_CLOUD_CPP_ENABLE=${ENABLED_FEATURES}"
  "-DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=OFF"
  "-DGOOGLE_CLOUD_CPP_ENABLE_WERROR=ON"
  "-DGOOGLE_CLOUD_CPP_DOXYGEN_CLANG_OPTIONS=-resource-dir=$(clang -print-resource-dir)"
  "-DGOOGLE_CLOUD_CPP_COMMIT_SHA=${COMMIT_SHA}"
  "-DGOOGLE_CLOUD_CPP_ENABLE_CLANG_ABI_COMPAT_17=ON"
)
if command -v /usr/local/bin/sccache >/dev/null 2>&1; then
  doc_args+=(
    -DCMAKE_CXX_COMPILER_LAUNCHER=/usr/local/bin/sccache
  )
fi

# If we're not on a release branch, point our links to "latest".
if ! grep -qP 'v\d+\.\d+\..*' <<<"${BRANCH_NAME}"; then
  doc_args+=("-DGOOGLE_CLOUD_CPP_USE_LATEST_FOR_REFDOC_LINKS=ON")
fi

# For PR and manual builds, publish to the staging site. For CI builds (release
# and otherwise), publish to the public URL.
docfx_bucket="docs-staging-v2-dev"
if [[ "${TRIGGER_TYPE}" == "ci" ]]; then
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

# Stage documentation in DocFX format for processing. go/cloud-rad for details.
function stage_docfx() {
  local feature="$1"
  local bucket="$2"
  local binary_dir="$3"
  local log="$4"
  local package="google-cloud-${feature}"
  local path="${binary_dir}/google/cloud/${feature}/docfx"
  if [[ "${feature}" == "common" ]]; then
    path="${binary_dir}/google/cloud/docfx"
  fi

  echo "path=${path}" >"${log}"
  if [[ ! -d "${path}" ]]; then
    echo "Directory not found: ${path}, skipping" >>"${log}"
    echo "SUCCESS" >>"${log}"
    return 0
  fi

  version="$(jq -r .version <"${path}/docs.metadata.json")"
  tar -C "${path}" -zcf "/tmp/cpp-${feature}-${version}.tar.gz" . >>"${log}" 2>&1
  export TIMEFORMAT="${feature} completed in %0lR"
  if time ci/retry-command.sh 3 120 gcloud storage cp "/tmp/cpp-${feature}-${version}.tar.gz" "gs://${bucket}" >>"${log}" 2>&1; then
    echo "SUCCESS" >>"${log}"
  fi
}
export -f stage_docfx # enables this function to be called from a subshell

io::log_h2 "Publishing DocFX"
io::log "branch:  ${BRANCH_NAME}"
io::log "bucket:  gs://${docfx_bucket}"

# Upload the documents for all features, including common. Some features do not
# have documentation, such as `experimental-storage_grpc`. These are harmless,
# as the `stage_docfx()` function skips missing directories without an error.
uploaded=(common)
uploaded+=("${FEATURE_LIST[@]}")
echo "${uploaded[@]}" | xargs -P "$(nproc)" -n 1 \
  bash -c "stage_docfx \"\${0}\" \"${docfx_bucket}\" cmake-out \"cmake-out/\${0}.docfx.log\""

errors=0
for feature in "${uploaded[@]}"; do
  log="cmake-out/${feature}.docfx.log"
  if [[ "$(tail -1 "${log}")" == "SUCCESS" ]]; then
    continue
  fi
  ((++errors))
  io::log_red "Error uploading documentation for ${feature}"
  cat "${log}"
done

echo
exit "${errors}"
