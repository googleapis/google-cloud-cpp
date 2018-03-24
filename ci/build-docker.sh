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

if [ "${TERM:-}" = "dumb" ]; then
  readonly COLOR_RED=""
  readonly COLOR_GREEN=""
  readonly COLOR_YELLOW=""
  readonly COLOR_RESET=""
else
  readonly COLOR_RED=$(tput setaf 1)
  readonly COLOR_GREEN=$(tput setaf 2)
  readonly COLOR_YELLOW=$(tput setaf 3)
  readonly COLOR_RESET=$(tput sgr0)
fi

# This script is supposed to run inside a Docker container, see
# ci/build-linux.sh for the expected setup.  The /v directory is a volume
# pointing to a (clean-ish) checkout of google-cloud-cpp:
(cd /v ; ./ci/check-style.sh)

# Run the configure / compile / test cycle inside a docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.
readonly IMAGE="cached-${DISTRO}-${DISTRO_VERSION}"
mkdir -p "build-output/${IMAGE}"
cd "build-output/${IMAGE}"

CMAKE_COMMAND="cmake"
if [ "${SCAN_BUILD}" = "yes" ]; then
  CMAKE_COMMAND="scan-build cmake"
fi

echo "travis_fold:start:configure-cmake"
${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" ${CMAKE_FLAGS:-} ../..
echo "travis_fold:end:configure-cmake"

# If scan-build is enabled, we need to manually compile the dependencies;
# otherwise, the static analyzer finds issues in them, and there is no way to
# ignore them.  When scan-build is not enabled, this is still useful because
# we can fold the output in Travis and make the log more interesting.
echo "${COLOR_YELLOW}Started dependency build at: $(date)${COLOR_RESET}"
echo "travis_fold:start:build-dependencies"
make -j ${NCPU} -C bigtable depends-local
echo "travis_fold:end:build-dependencies"
echo "${COLOR_YELLOW}Finished dependency build at: $(date)${COLOR_RESET}"

# If scan-build is enabled we build the smallest subset of things that is
# needed; otherwise, we pick errors from things we do not care about. With
# scan-build disabled we compile everything, to test the build as most
# developers will experience it.
echo "${COLOR_YELLOW}Started build at: $(date)${COLOR_RESET}"
if [ "${SCAN_BUILD}" = "yes" ]; then
  scan-build make -j ${NCPU} -C bigtable tests-local
else
  make -j ${NCPU} all
fi
echo "${COLOR_YELLOW}Finished build at: $(date)${COLOR_RESET}"

# Run the tests and output any failures.
CTEST_OUTPUT_ON_FAILURE=1 make -j ${NCPU} test

# Run the integration tests.
(cd bigtable/tests && /v/bigtable/tests/run_integration_tests_emulator.sh)

# Some of the sanitizers only emit errors and do not change the error code
# of the tests, find any such errors and report them as a build failure.
echo
echo "Searching for sanitizer errors in the test log:"
echo
if grep -e '/v/.*\.cc:[0-9][0-9]*' \
       Testing/Temporary/LastTest.log; then
  echo
  echo "some sanitizer errors found."
  exit 1
else
  echo "no sanitizer errors found."
fi

# Test the install rule and that the installation works.
if [ "${TEST_INSTALL}" = "yes" ]; then
  echo
  echo "${COLOR_YELLOW}Testing install rule.${COLOR_RESET}"
  make install
  echo
  echo "${COLOR_YELLOW}Test installed libraries using make(1).${COLOR_RESET}"
  make -C /v/ci/test-install all
  echo
  echo "${COLOR_YELLOW}Test installed libraries using cmake(1).${COLOR_RESET}"
  cd /v/ci/test-install
  CMAKE_PREFIX_PATH=/usr/local/share cmake -H. -B.build
  cmake --build .build
fi

# If document generation is enabled, run it now.
if [ "${GENERATE_DOCS}" = "yes" ]; then
  make doxygen-docs
fi

# Collect the output from the Clang static analyzer and provide instructions to
# the developers on how to do that locally.
if [ "${SCAN_BUILD:-}" = "yes" ]; then
  if [ -n "$(ls -1d /tmp/scan-build-* 2>/dev/null)" ]; then
    cp -r /tmp/scan-build-* /v/scan-build-output
  fi
  if [ -r scan-build-output/index.html ]; then
    cat <<_EOF_;

${COLOR_RED}
scan-build detected errors.  Please read the log for details. To
run scan-build locally and examine the HTML output install and configure Docker,
then run:

DISTRO=ubuntu DISTRO_VERSION=16.04 SCAN_BUILD=yes NCPU=8 TRAVIS_OS_NAME=linux CXX=clang++ CC=clang ./ci/build-linux.sh

The HTML output will be copied into the scan-build-output subdirectory.
${COLOR_RESET}
_EOF_
    exit 1
  else
    echo
    echo "${COLOR_GREEN}scan-build completed without errors.${COLOR_RESET}"
  fi
fi
