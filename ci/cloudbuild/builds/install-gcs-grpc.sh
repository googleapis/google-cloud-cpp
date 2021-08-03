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

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/quickstart.sh

export CC=gcc
export CXX=g++

INSTALL_PREFIX="/var/tmp/google-cloud-cpp"
cmake -GNinja \
  -DGOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -S . -B cmake-out
cmake --build cmake-out
env -C cmake-out ctest -LE "integration-test" --parallel "$(nproc)"
cmake --build cmake-out --target install

# Tests the installed artifacts by building and running the quickstarts.
quickstart::build_gcs_grpc_quickstart "${INSTALL_PREFIX}"
quickstart::run_gcs_grpc_quickstart "${INSTALL_PREFIX}"
