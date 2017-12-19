#!/usr/bin/env bash
#
# Copyright 2017 Google Inc.
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

# Run the configure / compile / test cycle inside a docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.
mkdir -p gccpp/build-output
cd gccpp/build-output

CMAKE_COMMAND="cmake"
if [ "${SCAN_BUILD}" = "yes" ]; then
  CMAKE_COMMAND="scan-build cmake"
fi
${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" ${CMAKE_FLAGS:-} ..

# If scan-build is enabled we need to manually compile the dependencies,
# otherwise the static analyzer finds issues in them, and there is no way to
# ignore them.
if [ "${SCAN_BUILD}" = "yes" ]; then
  make -j ${NCPU} -C bigtable googleapis
  scan-build make -j ${NCPU} -C bigtable all
else
  make -j ${NCPU} all
fi

CTEST_OUTPUT_ON_FAILURE=1 make -j ${NCPU} test

# Some of the sanitizers only emit errors and do not change the error code
# of the tests, find any such errors and report them as a build failure.
echo
echo "Searching for sanitizer errors in the test log:"
echo
if grep -e '/var/tmp/build/gccpp/.*\.cc:[0-9][0-9]*' \
       Testing/Temporary/LastTest.log; then
  echo
  echo "some sanitizer errors found."
  exit 1
else
  echo "no sanitizer errors found."
fi

# if document generation is enabled, run it now.
if [ "${GENERATE_DOCS}" = "yes" ]; then
  cd ../bigtable
  doxygen doc/Doxyfile
fi
