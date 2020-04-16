#!/usr/bin/env bash
# Copyright 2020 Google LLC
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

if [[ $# -ne 1 ]]; then
  echo "Usage: $(basename "$0") <project-root>"
  exit 1
fi

readonly PROJECT_ROOT="$1"

source "${PROJECT_ROOT}/ci/colors.sh"
source "${PROJECT_ROOT}/ci/etc/integration-tests-config.sh"
source "${PROJECT_ROOT}/ci/etc/quickstart-config.sh"

echo "================================================================"
log_yellow "Update or install dependencies."

# Clone and build vcpkg
vcpkg_dir="cmake-out/vcpkg-quickstart"
if [[ -d "${vcpkg_dir}" ]]; then
  git -C "${vcpkg_dir}" pull --quiet
else
  git clone --quiet --depth 10 https://github.com/microsoft/vcpkg.git "${vcpkg_dir}"
fi

if [[ -d "cmake-out/vcpkg-quickstart-cache" && ! -d \
  "${vcpkg_dir}/installed" ]]; then
  cp -r "cmake-out/vcpkg-quickstart-cache" "${vcpkg_dir}/installed"
fi

(
  cd "${vcpkg_dir}"
  ./bootstrap-vcpkg.sh
  ./vcpkg remove --outdated --recurse
  ./vcpkg install google-cloud-cpp
)
# Use the new installed/ directory to create the cache.
rm -fr "cmake-out/vcpkg-quickstart-cache"
cp -r "${vcpkg_dir}/installed" "cmake-out/vcpkg-quickstart-cache"

run_quickstart="false"
readonly CONFIG_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly CREDENTIALS_FILE="${CONFIG_DIR}/kokoro-run-key.json"
readonly ROOTS_PEM_SOURCE="https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem"
if [[ -r "${CREDENTIALS_FILE}" ]]; then
  if [[ -r "${CONFIG_DIR}/roots.pem" ]]; then
    run_quickstart="true"
  elif wget -O "${CONFIG_DIR}/roots.pem" -q "${ROOTS_PEM_SOURCE}"; then
    run_quickstart="true"
  fi
fi
readonly run_quickstart

echo "================================================================"
cd "${PROJECT_ROOT}"
cmake_flags=(
  "-DCMAKE_TOOLCHAIN_FILE=${PROJECT_ROOT}/${vcpkg_dir}/scripts/buildsystems/vcpkg.cmake"
)

build_quickstart() {
  local -r library="$1"

  echo
  log_yellow "Configure CMake for ${library}'s quickstart."
  source_dir="google/cloud/${library}/quickstart"
  binary_dir="cmake-out/quickstart-${library}"
  cmake "-H${source_dir}" "-B${binary_dir}" "${cmake_flags[@]}"

  echo
  log_yellow "Compiling ${library}'s quickstart."
  cmake --build "${binary_dir}"

  if [[ "${run_quickstart}" == "true" ]]; then
    echo
    log_yellow "Running ${library}'s quickstart."
    args=()
    while IFS="" read -r line; do
      args+=("${line}")
    done < <(quickstart_arguments "${library}")
    env "GOOGLE_APPLICATION_CREDENTIALS=${CREDENTIALS_FILE}" \
      "GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=${CONFIG_DIR}/roots.pem" \
      "${binary_dir}/quickstart" "${args[@]}"
  fi
}

errors=""
for library in $(quickstart_libraries); do
  echo
  echo "================================================================"
  log_yellow "Building ${library}'s quickstart"
  if ! build_quickstart "${library}"; then
    log_red "Building ${library}'s quickstart failed"
    errors="${errors} ${library}"
  else
    log_green "Building ${library}'s quickstart was successful"
  fi
done

echo "================================================================"
if [[ -z "${errors}" ]]; then
  log_green "All quickstart builds were successful"
else
  log_red "Build failed for ${errors}"
  exit 1
fi

exit 0
