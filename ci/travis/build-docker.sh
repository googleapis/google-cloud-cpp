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

readonly BINDIR="$(dirname $0)"
source "${BINDIR}/../colors.sh"

# This script is supposed to run inside a Docker container, see
# ci/travis/build-linux.sh for the expected setup.  The /v directory is a volume
# pointing to a (clean-ish) checkout of google-cloud-cpp:
(cd /v ; ./ci/check-style.sh)

# Run the configure / compile / test cycle inside a docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.
readonly IMAGE="cached-${DISTRO}-${DISTRO_VERSION}"
readonly BUILD_DIR="build-output/${IMAGE}"

CMAKE_COMMAND="cmake"
if [ "${SCAN_BUILD}" = "yes" ]; then
  CMAKE_COMMAND="scan-build --use-cc=${CC} --use-c++=${CXX} cmake"
fi

ccache_command="$(which ccache)"

function install_crc32c {
  # Install googletest.
  echo "${COLOR_YELLOW}Installing Crc32c $(date)${COLOR_RESET}"
  wget -q https://github.com/google/crc32c/archive/1.0.5.tar.gz
  tar -xf 1.0.5.tar.gz
  cd crc32c-1.0.5
  env CXX="${cached_cxx}" CC="${cached_cc}"  cmake \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      ${CMAKE_FLAGS} \
      -H. -B.build/crc32c
  cmake --build .build/crc32c --target install -- -j ${NCPU}
  ldconfig
}

function install_protobuf {
  # Install protobuf using CMake.  Some distributions include protobuf, gRPC
  # requires 3.4.x or newer, and many of those distribution use older versions.
  # We need to install both the debug and Release version because:
  # - When using pkg-config, only the release version works, the pkg-config
  #   file is hard-coded to search for `libprotobuf.so` (or `.a`)
  # - When using CMake, only the version compiled with the same CMAKE_BUILD_TYPE
  #   as the dependent (gRPC or google-cloud-cpp) works.
  echo "${COLOR_YELLOW}Installing protobuf $(date)${COLOR_RESET}"
  wget -q https://github.com/google/protobuf/archive/v3.5.2.tar.gz
  tar -xf v3.5.2.tar.gz
  cd protobuf-3.5.2/cmake
  for build_type in "Debug" "Release"; do
    env CXX="${cached_cxx}" CC="${cached_cc}" cmake \
        -DCMAKE_BUILD_TYPE="${build_type}" \
        -Dprotobuf_BUILD_TESTS=OFF \
        ${CMAKE_FLAGS} \
        -H. -B.build-${build_type}
    cmake --build .build-${build_type} --target install -- -j ${NCPU}
  done
  ldconfig
}

function install_c_ares {
  # Many distributions include c-ares, but they do not include the CMake support
  # files for the library, so manually install it.  c-ares requires two install
  # steps because (1) the CMake-based build does not install pkg-config files,
  # and (2) the Makefile-based build does not install CMake config files.
  echo "${COLOR_YELLOW}Installing c-ares $(date)${COLOR_RESET}"
  apt-get remove -y libc-ares-dev libc-ares2
  wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz
  tar -xf cares-1_14_0.tar.gz
  cd c-ares-cares-1_14_0
  env CXX="${cached_cxx}" CC="${cached_cc}" cmake \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      ${CMAKE_FLAGS} \
      -H. -B.build; \
  cmake --build .build --target install -- -j ${NCPU}
  ./buildconf
  ./configure
  install -m 644 -D -t /usr/local/lib/pkgconfig libcares.pc
  ldconfig
}

function install_grpc {
  # Install gRPC. Note that we use the system's zlib and ssl libraries.
  # For similar reasons to c-ares (see above), we need two install steps.
  echo "${COLOR_YELLOW}Installing gRPC $(date)${COLOR_RESET}"
  wget -q https://github.com/grpc/grpc/archive/v1.14.0.tar.gz
  tar -xf v1.14.0.tar.gz
  cd grpc-1.14.0
  env CXX="${cached_cxx}" CC="${cached_cc}"  cmake \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_ZLIB_PROVIDER=package \
      -DgRPC_SSL_PROVIDER=package \
      -DgRPC_CARES_PROVIDER=package \
      -DgRPC_PROTOBUF_PROVIDER=package \
      ${CMAKE_FLAGS} \
      -H. -B.build/grpc
  cmake --build .build/grpc --target install -- -j ${NCPU}
  make install-pkg-config_c install-pkg-config_cxx install-certs
  ldconfig
}

function install_googletest {
  # Install googletest.
  echo "${COLOR_YELLOW}Installing googletest $(date)${COLOR_RESET}"
  wget -q https://github.com/google/googletest/archive/release-1.8.1.tar.gz
  tar -xf release-1.8.1.tar.gz
  cd googletest-release-1.8.1
  env CXX="${cached_cxx}" CC="${cached_cc}"  cmake \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      ${CMAKE_FLAGS} \
      -H. -B.build/googletest
  cmake --build .build/googletest --target install -- -j ${NCPU}
  ldconfig
}

if [ "${TEST_INSTALL:-}" = "yes" -o "${SCAN_BUILD:-}" = "yes" ]; then
  echo
  echo "${COLOR_YELLOW}Started dependency install at: $(date)${COLOR_RESET}"
  echo
  echo "travis_fold:start:install-dependencies"
  echo
  mkdir -p /var/tmp/build-dependencies
  cached_cxx="${CXX}"
  cached_cc="${CC}"
  if [ -n "${ccache_command}" ]; then
    cached_cxx="${ccache_command} ${CXX}"
    cached_cc="${ccache_command} ${CC}"
  fi
  (cd /var/tmp/build-dependencies; install_crc32c)
  (cd /var/tmp/build-dependencies; install_protobuf)
  (cd /var/tmp/build-dependencies; install_c_ares)
  (cd /var/tmp/build-dependencies; install_grpc)
  (cd /var/tmp/build-dependencies; install_googletest)
  ${ccache_command} --show-stats
  echo
  echo "${COLOR_YELLOW}Finished dependency install at: $(date)${COLOR_RESET}"
  echo
  echo "travis_fold:end:install-dependencies"
  echo
fi

echo "${COLOR_YELLOW}Started CMake config at: $(date)${COLOR_RESET}"
echo "travis_fold:start:configure-cmake"
# Tweak configuration for TEST_INSTALL=yes and SCAN_BUILD=yes builds.
cmake_install_flags=""
if [ "${TEST_INSTALL:-}" = "yes" ]; then
  cmake_install_flags=-DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package
fi

if [ "${SCAN_BUILD:-}" = "yes" ]; then
  cmake_install_flags=-DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package
  cmake_install_flags="${cmake_install_flags} -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=OFF"
fi

${CMAKE_COMMAND} \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    ${cmake_install_flags} \
    ${CMAKE_FLAGS:-} \
    -H. \
    -B"${BUILD_DIR}"
echo
echo "travis_fold:end:configure-cmake"
echo "${COLOR_YELLOW}Finished CMake config at: $(date)${COLOR_RESET}"

# If scan-build is enabled, we need to manually compile the dependencies;
# otherwise, the static analyzer finds issues in them, and there is no way to
# ignore them.  When scan-build is not enabled, this is still useful because
# we can fold the output in Travis and make the log more interesting.
echo "${COLOR_YELLOW}Started dependency build at: $(date)${COLOR_RESET}"
echo "travis_fold:start:build-dependencies"
echo
cmake --build "${BUILD_DIR}" --target skip-scanbuild-targets -- -j ${NCPU}
echo
echo "travis_fold:end:build-dependencies"
echo "${COLOR_YELLOW}Finished dependency build at: $(date)${COLOR_RESET}"

# If scan-build is enabled we build the smallest subset of things that is
# needed; otherwise, we pick errors from things we do not care about. With
# scan-build disabled we compile everything, to test the build as most
# developers will experience it.
echo "${COLOR_YELLOW}Started build at: $(date)${COLOR_RESET}"
${CMAKE_COMMAND} --build "${BUILD_DIR}" -- -j ${NCPU}
echo "${COLOR_YELLOW}Finished build at: $(date)${COLOR_RESET}"

# If ccache is enabled we want to zero out the statistics because otherwise
# Travis needs to rebuild the cache each time, and that slows down the build
# unnecessarily.
if [ -n "${ccache_command}" ]; then
  echo
  echo "${COLOR_YELLOW}Print and clearing ccache stats: $(date)${COLOR_RESET}"
  ${ccache_command} --show-stats
  ${ccache_command} --zero-stats --cleanup --max-size=1Gi
fi

# Run the tests and output any failures.
echo
echo "${COLOR_YELLOW}Running unit and integration tests $(date)${COLOR_RESET}"
echo
cd "${BUILD_DIR}"
ctest --output-on-failure

# Run the integration tests. Not all projects have them, so just iterate over
# the ones that do.
for subdir in google/cloud google/cloud/bigtable google/cloud/storage; do
  echo
  echo "${COLOR_GREEN}Running integration tests for ${subdir}${COLOR_RESET}"
  /v/${subdir}/ci/run_integration_tests.sh
done

# Test the install rule and that the installation works.
if [ "${TEST_INSTALL:-}" = "yes" ]; then
  echo
  echo "${COLOR_YELLOW}Testing install rule.${COLOR_RESET}"
  cmake --build . --target install
  echo
  echo "${COLOR_YELLOW}Test installed libraries using cmake(1).${COLOR_RESET}"
  readonly TEST_INSTALL_DIR=/v/ci/test-install
  readonly TEST_INSTALL_CMAKE_OUTPUT_DIR=/v/build-output/test-install-cmake
  readonly TEST_INSTALL_MAKE_OUTPUT_DIR=/v/build-output/test-install-make
  cmake -H"${TEST_INSTALL_DIR}" -B"${TEST_INSTALL_CMAKE_OUTPUT_DIR}"
  cmake --build "${TEST_INSTALL_CMAKE_OUTPUT_DIR}"
  echo
  echo "${COLOR_YELLOW}Test installed libraries using make(1).${COLOR_RESET}"
  mkdir -p "${TEST_INSTALL_MAKE_OUTPUT_DIR}"
  make -C "${TEST_INSTALL_MAKE_OUTPUT_DIR}" -f"${TEST_INSTALL_DIR}/Makefile" VPATH="${TEST_INSTALL_DIR}"

  # Checking the ABI requires installation, so this is the first opportunity to
  # run the check.
  (cd /v ; ./ci/check-abi.sh)

  # Also verify that the install directory does not get unexpected files or
  # directories installed.
  echo
  echo "${COLOR_YELLOW}Verify installed headers created only" \
      " expected directories.${COLOR_RESET}"
  cmake --build . --target install -- DESTDIR=/var/tmp/staging
  if comm -23 \
      <(find /var/tmp/staging/usr/local/include/google/cloud -type d | sort) \
      <(echo /var/tmp/staging/usr/local/include/google/cloud ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/bigtable ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/bigtable/internal ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/firestore ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/internal ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/storage ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/storage/internal ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/storage/oauth2 ; \
        /bin/true) | grep -q /var/tmp; then
      echo "${COLOR_YELLOW}Installed directories do not match expectation.${COLOR_RESET}"
      echo "${COLOR_RED}Found:"
      find /var/tmp/staging/usr/local/include/google/cloud -type d | sort
      echo "${COLOR_RESET}"
      /bin/false
   fi
fi

# If document generation is enabled, run it now.
if [ "${GENERATE_DOCS}" = "yes" ]; then
  echo
  echo "${COLOR_YELLOW}Generating Doxygen documentation at: $(date).${COLOR_RESET}"
  cmake --build . --target doxygen-docs
fi

# Some of the sanitizers only emit errors and do not change the error code
# of the tests, find any such errors and report them as a build failure.
echo
echo -n "Searching for sanitizer errors in the test log: "
if grep -qe '^/v/.*\.cc:[0-9][0-9]*' Testing/Temporary/LastTest.log; then
  echo "${COLOR_RED}some sanitizer errors found."
  echo
  grep -e '^/v/.*\.cc:[0-9][0-9]*' Testing/Temporary/LastTest.log
  echo "${COLOR_RESET}"
  exit 1
else
  echo "${COLOR_GREEN}no sanitizer errors found.${COLOR_RESET}"
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

DISTRO=ubuntu DISTRO_VERSION=16.04 SCAN_BUILD=yes NCPU=8 TRAVIS_OS_NAME=linux \
    CXX=clang++ CC=clang ./ci/travis/build-linux.sh

The HTML output will be copied into the scan-build-output subdirectory.
${COLOR_RESET}
_EOF_
    exit 1
  else
    echo
    echo "${COLOR_GREEN}scan-build completed without errors.${COLOR_RESET}"
  fi
fi
