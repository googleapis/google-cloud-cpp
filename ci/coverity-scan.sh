#!/usr/bin/env bash
#
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

set -eu

if [[ -z "$(command -v cov-build)" ]]; then
  echo "This script requires the coverity scan tool (cov-build) in PATH."
  echo "Please download the tool, make sure your PATH includes the directory"
  echo "that contains it and try again."
  echo "For more details, see: https://scan.coverity.com/download"
  exit 1
fi

# Coverity scan is pre-configured to run with gcc, so do the same here.
export CXX=g++
export CC=gcc

# Running with coverity-scan and ccache seems like a bad idea, disable ccache.
# Also build in Debug mode because building in Release mode takes too long.
cmake -H. -B.coverity \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=OFF

# Run coverity scan over our code.
cov-build --dir cov-int cmake --build .coverity -- -j "$(nproc)"

if [[ -z "${COVERITY_SCAN_TOKEN}" ]]; then
  echo "COVERITY_SCAN_TOKEN is not defined, skipping upload of results."
  exit 0
fi

if [[ -z "${COVERITY_SCAN_EMAIL}" ]]; then
  echo "COVERITY_SCAN_EMAIL is not defined, skipping upload of results."
  exit 0
fi

# Create a tarball with the build results. Print something because this takes
# a while.
/bin/echo -n "Creating a tarball with the Coverity Scan results." \
  "This may take several minutes..."
tar caf google-cloud-cpp.tar.xz cov-int
/bin/echo "DONE"

curl --form "token=${COVERITY_SCAN_TOKEN}" \
  --form email="${COVERITY_SCAN_EMAIL}" \
  --form file=@google-cloud-cpp.tar.xz \
  --form version="master" \
  --form description="Automatically Compiled Coverity Scan" \
  https://scan.coverity.com/builds?project=googleapis%2Fgoogle-cloud-cpp
