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

if [[ $# -ne 3 ]]; then
  echo "Usage: $(basename "$0") <project-root> <source-directory> <binary-directory>"
  exit 1
fi

readonly PROJECT_ROOT="$1"
readonly SOURCE_DIR="$2"
readonly BINARY_DIR="$3"

NCPU="$(sysctl -n hw.logicalcpu)"
readonly NCPU

source "${PROJECT_ROOT}/ci/colors.sh"

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): Update or install dependencies.${COLOR_RESET}"
brew install libressl

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): ccache stats${COLOR_RESET}"
ccache --show-stats
ccache --zero-stats

echo "================================================================"
cd "${PROJECT_ROOT}"
export OPENSSL_ROOT_DIR=/usr/local/opt/libressl
cmake_flags=(
  "-DCMAKE_INSTALL_PREFIX=$HOME/staging"
  "-DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON"
)

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): Configure CMake.${COLOR_RESET}"
cmake "-H${SOURCE_DIR}" "-B${BINARY_DIR}" "${cmake_flags[@]}"

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): Compiling with ${NCPU} cpus.${COLOR_RESET}"
cmake --build "${BINARY_DIR}" -- -j "${NCPU}"

# When user a super-build the tests are hidden in a subdirectory. We can tell
# that ${BINARY_DIR} does not have the tests by checking for this file:
if [[ -r "${BINARY_DIR}/CTestTestfile.cmake" ]]; then
  # If the file is not present, then this is a super build, which automatically
  # run the tests anyway, no need to run them again.
  echo "================================================================"
  echo "${COLOR_YELLOW}$(date -u): Running unit tests.${COLOR_RESET}"
  (cd "${BINARY_DIR}"; ctest \
      -LE integration-tests \
      --output-on-failure -j "${NCPU}")
  echo "================================================================"
fi

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): ccache stats${COLOR_RESET}"
ccache --show-stats
ccache --zero-stats

echo "================================================================"
echo "${COLOR_GREEN}$(date -u): Build finished sucessfully${COLOR_RESET}"
echo "================================================================"

exit 0
