#!/usr/bin/env bash
#
# Copyright 2018 Google LLC
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

if [ -z "${PROJECT_ROOT+x}" ]; then
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/define-dump-log.sh"

readonly BUILD_OUTPUT="build-output/cached-${DISTRO}-${DISTRO_VERSION}"
# Dump the emulator log file. Tests run in the google/cloud/bigtable/tests directory.
echo
dump_log "${BUILD_OUTPUT}/google/cloud/bigtable/tests/emulator.log"
dump_log "${BUILD_OUTPUT}/google/cloud/bigtable/tests/instance-admin-emulator.log"

# This script runs on macOS and Linux, there are no analysis steps executed by
# the macOS builds, so we can safely exit here unless we are running Linux.
if [ "${TRAVIS_OS_NAME}" != "linux" ]; then
  echo "Not a Linux-based build, skipping Linux-specific log dumping steps."
  exit 0
fi

# Find any analysis reports, currently ABI checks and Clang static analysis are
# the two things that produce them. Note that the Clang static analysis reports
# are copied into the scan-build-output directory by the build-docker.sh script.
readonly ABI_CHECK_REPORTS="$(find "${BUILD_OUTPUT}" -name 'compat_report.html')"
readonly SCAN_BUILD_REPORTS="$(find scan-build-output/ -name '*.html' 2>/dev/null)"

if [ -z "${ABI_CHECK_REPORTS}" ] && [ -z "${SCAN_BUILD_REPORTS}" ]; then
  echo "No analysis reports found, exit scripts with success."
  exit 0
fi

# If w3m is installed there is nothing to do.
if which w3m >/dev/null 2>&1; then
  echo "Found w3m already installed."
else
  # Try to install a HTML renderer, if this fails the script will exit.
  # Note that this runs on the Travis VM, under Ubuntu, so the command
  # to install things is well-known:
  sudo apt install -y w3m
fi

for report in ${ABI_CHECK_REPORTS} ${SCAN_BUILD_REPORTS}; do
  echo
  echo "================ ${report} ================"
  w3m -dump "${report}"
done
