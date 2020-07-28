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

source "$(dirname "$0")/../../lib/init.sh"
source module lib/io.sh

if [[ $# != 2 ]]; then
  echo "Usage: $(basename "$0") <binary-directory> <library>"
  exit 1
fi

readonly BINARY_DIR="$1"
readonly LIBRARY="$2"

# This script is invoked several times in parallel using xargs -P. To make the
# interleaved output from the commands below more readable we'll prepend the
# library name to every line of stdout and stderr for the rest of this script.
exec > >(sed -e "s,^,${LIBRARY}: ,")
exec 2>&1

# This script is only run on a Fedora-based build, so it's fine that this
# setting is not portable.
export PKG_CONFIG_PATH="/var/tmp/staging/lib64/pkgconfig:/usr/local/lib64/pkgconfig"

readonly LIBDIR="$(pkg-config "${LIBRARY}" --variable=libdir)"
readonly INCLUDEDIR="$(pkg-config "${LIBRARY}" --variable=includedir)"
readonly LIBRARY_PATH="${LIBDIR}/lib${LIBRARY}.so"
readonly ACTUAL_DUMP_FILE="${LIBRARY}.actual.abi.dump"
readonly ACTUAL_DUMP_PATH="${BINARY_DIR}/${ACTUAL_DUMP_FILE}"
readonly EXPECTED_DUMP_FILE="${LIBRARY}.expected.abi.dump"
readonly EXPECTED_DUMP_PATH="${PROJECT_ROOT}/ci/test-abi/${EXPECTED_DUMP_FILE}.gz"

# If we were asked to update the expected ABI files, generate them then exit.
if [[ "${UPDATE_ABI}" == "yes" ]]; then
  io::log "Updating expected ABI dump for ${LIBRARY}"
  abi-dumper "${LIBRARY_PATH}" \
    -public-headers "${INCLUDEDIR}" \
    -lver "expected" \
    -o "${ACTUAL_DUMP_PATH}" &&
    gzip -c "${ACTUAL_DUMP_PATH}" >"${EXPECTED_DUMP_PATH}"
  exit $?
fi

io::log "Checking ABI of ${LIBRARY}"
abi-dumper "${LIBRARY_PATH}" \
  -public-headers "${INCLUDEDIR}" \
  -lver "actual" \
  -o "${ACTUAL_DUMP_PATH}"

cd "${BINARY_DIR}"
zcat "${EXPECTED_DUMP_PATH}" >"${EXPECTED_DUMP_FILE}"
# We ignore all symbols in internal namespaces, because these are not part
# of our public API. We do this by specifying a regex that matches against
# the mangled symbol names. For example, 8 is the number of characters in
# the string "internal", and it should again be followed by some other
# number indicating the length of the symbol within the "internal"
# namespace. See: https://en.wikipedia.org/wiki/Name_mangling
abi-compliance-checker \
  -skip-internal-symbols "(8internal|15pubsub_internal)\d" \
  -src -l "${LIBRARY}" -old "${EXPECTED_DUMP_FILE}" -new "${ACTUAL_DUMP_FILE}"
