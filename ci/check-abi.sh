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

readonly BINDIR="$(dirname $0)"
source "${BINDIR}/colors.sh"
readonly PROJECT_ROOT="$(cd ${BINDIR}/..; pwd)"

if [ "${CHECK_ABI:-}" != "yes" ]; then
  echo
  echo "${COLOR_YELLOW}Skipping ABI check as it is disabled for this build." \
      "${COLOR_RESET}"
  exit 0
fi

readonly IMAGE="cached-${DISTRO}-${DISTRO_VERSION}"
for library in google_cloud_cpp_common bigtable_client; do
  echo
  echo "${COLOR_YELLOW}Checking ABI for ${library} library.${COLOR_RESET}"
  libdir="$(pkg-config ${library} --variable=libdir)"
  includedir="$(pkg-config ${library} --variable=includedir)"
  version="$(pkg-config ${library} --modversion)"
  new_dump_file="${PROJECT_ROOT}/build-output/${IMAGE}/${library}.actual.abi.dump"
  old_dump_file="${PROJECT_ROOT}/ci/test-abi/${library}.expected.abi.dump"
  abi-dumper "${libdir}/lib${library}.so" \
      -public-headers "${includedir}" \
      -lver "${version}" \
      -o ${new_dump_file}
  (cd "build-output/${IMAGE}" ; abi-compliance-checker -l ${library} \
      -old "${old_dump_file}" \
      -new "${new_dump_file}")
done
