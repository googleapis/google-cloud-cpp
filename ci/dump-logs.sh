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

readonly BUILD_OUTPUT="build-output/cached-${DISTRO}-${DISTRO_VERSION}"
# Dump the emulator log file. Tests run in the google/cloud/bigtable/tests directory.
echo
echo "================ emulator.log ================"
cat "${BUILD_OUTPUT}/google/cloud/bigtable/tests/emulator.log" >&2 || true
echo
echo "================ instance-admin-emulator.log ================"
cat "${BUILD_OUTPUT}/google/cloud/bigtable/tests/instance-admin-emulator.log" >&2 || true

readonly ABI_CHECK_REPORTS=$(find ${BUILD_OUTPUT} -name 'compat_report.html')
readonly SCAN_BUILD_REPORTS=$(find scan-build-output/ -name '*.html' 2>/dev/null)

if [ -n "${ABI_CHECK_REPORTS}" -o -n "${SCAN_BUILD_REPORTS}" ]; then
  # Try to install a HTML renderer.
  apt install -y w3m
fi

for report in ${ABI_CHECK_REPORTS} ${SCAN_BUILD_REPORTS}; do
  echo
  echo "================ ${report} ================"
  w3m -dump "${report}"
done
