#!/bin/bash
#
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Usage:
#   $ deps-cache.sh [-c] [<google_cloud_cpp_deps.bzl>]
#   $ deps-cache.sh -p [<google_cloud_cpp_deps.bzl>]
#
#   Args:
#     google_cloud_cpp_deps.bzl    Path to the .bzl file that specifies the
#                                  HTTP archives that we cache in GCS. By
#                                  default uses "google_cloud_cpp_deps.bzl"
#                                  in the same directory as this script.
#
#   Options:
#     -c     Check that the HTTP archives are correctly cached (the default)
#     -p     Populate the cache with current copies of the HTTP archives
#     -h     Print help message
#
# Note:
#   This does not edit the .bzl file to add cache files as URLs. It only
#   checks/populates the cached files themselves, so they're ready for use.

set -euo pipefail

# Extracts all the documentation at the top of this file as the usage text.
USAGE="$(sed -n '/^# Usage:/,/^$/s/^# \?//p' "$0")"
readonly USAGE

declare -i POPULATE_FLAG=0
while getopts "chp" opt "$@"; do
  case "${opt}" in
    c)
      POPULATE_FLAG=
      ;;
    p)
      POPULATE_FLAG=1
      ;;
    h)
      echo "${USAGE}"
      exit 0
      ;;
    *)
      echo "${USAGE}"
      exit 1
      ;;
  esac
done
shift $((OPTIND - 1))
readonly POPULATE_FLAG

BZL_FILE=
case $# in
  0)
    BZL_FILE="$(dirname "$0")/google_cloud_cpp_deps.bzl"
    ;;
  1)
    BZL_FILE="$1"
    shift
    ;;
  *)
    echo "${USAGE}"
    exit 1
    ;;
esac
readonly BZL_FILE

PROGRAM="$(basename "$0" .sh)"
TMPDIR=$(mktemp -d --tmpdir "${PROGRAM}.XXXXXXXXXX")
readonly PROGRAM TMPDIR
trap 'rm -fr "${TMPDIR}"' EXIT

# Extract the http_archive information from ${BZL_FILE}.
cat >"${TMPDIR}/script" <<'EOT'
/^ *maybe($/ {
  : maybe
  n
  /^ *name = ".*",$/ {
    s/^ *name = "\(.*\)",$/NAME="\1"/
    H
    b maybe
  }
  /^ *urls = \[$/ {
    : urls
    n
    /^ *"\(.*\),$/ {
      \@"https://storage.googleapis.com/@ b urls
      \@"https://mirror.bazel.build/@ b urls
      s/^ *"\(.*\)",$/ URL="\1"/
      H
      b urls
    }
    /^ *#/ b urls
    /^ *\],$/ {
      b maybe
    }
    Q 2
  }
  /^ *sha256 = ".*",$/ {
    s/^ *sha256 = "\(.*\)",$/ SHA256="\1"/
    H
    b maybe
  }
  /^ *#/ b maybe
  /^ *http_archive,$/ b maybe
  /^ *strip_prefix = ".*",$/ b maybe
  /^ *build_file = .*,$/ b maybe
  /^ *patch/ b maybe
  /^ *)$/ {
    s/.*//
    x
    s/\n//g
    p
    b
  }
  Q 1
}
EOT
coproc { sed -n -f "${TMPDIR}/script" "${BZL_FILE}"; }
WAIT_PID="${COPROC_PID}"
readarray ARCHIVES <&"${COPROC[0]}"
if wait "${WAIT_PID}"; then
  : # "! wait" would clear $?
else
  echo "${PROGRAM}: ${BZL_FILE}: Parse error ($?)"
  exit 1
fi

# Download archives and verify checksums.
declare -i FETCH_ERRORS=0
for ARCHIVE in "${ARCHIVES[@]}"; do
  eval "${ARCHIVE}" # sets ${NAME} ${URL} ${SHA256}
  BASENAME=$(basename "${URL}")

  mkdir -p "${TMPDIR}/source/${NAME}"
  SOURCE="${TMPDIR}/source/${NAME}/${BASENAME}"
  echo "[ Fetching ${URL} ]"
  if ! curl -sSL -o "${SOURCE}" "${URL}"; then
    ((++FETCH_ERRORS))
  else
    CHKSUM=$(sha256sum "${SOURCE}" | sed 's/ .*//')
    if [[ "${CHKSUM}" != "${SHA256}" ]]; then
      echo "${PROGRAM}: ${NAME}: Checksum mismatch"
      ((++FETCH_ERRORS))
    fi
  fi
  echo ''
done
if ((FETCH_ERRORS != 0)); then
  echo '[ Fetch error(s) ]'
  exit 1
fi

readonly BUCKET="cloud-cpp-community-archive"

# Verify the GCS cache matches, populating if requested.
declare -i VERIFY_ERRORS=0
for ARCHIVE in "${ARCHIVES[@]}"; do
  eval "${ARCHIVE}" # sets ${NAME} ${URL} ${SHA256}
  BASENAME=$(basename "${URL}")
  SOURCE="${TMPDIR}/source/${NAME}/${BASENAME}"
  GCS="https://storage.googleapis.com/${BUCKET}/${NAME}/${BASENAME}"

  mkdir -p "${TMPDIR}/cached/${NAME}"
  CACHED="${TMPDIR}/cached/${NAME}/${BASENAME}"
  echo "[ Verifying ${GCS} ]"
  declare -i VERIFY_ERROR=0
  if ! curl -sSL -o "${CACHED}" "${GCS}"; then
    ((++VERIFY_ERROR))
  elif ! cmp --silent "${SOURCE}" "${CACHED}"; then
    ((++VERIFY_ERROR))
  fi
  if ((VERIFY_ERROR != 0 && POPULATE_FLAG != 0)); then
    UPLOAD="gs://${BUCKET}/${NAME}/${BASENAME}"
    echo "[ Uploading ${UPLOAD} ]"
    gsutil -q cp "${SOURCE}" "${UPLOAD}"
    echo "[ Reverifying ${GCS} ]"
    VERIFY_ERROR=0
    if ! curl -sSL -o "${CACHED}" "${GCS}"; then
      ((++VERIFY_ERROR))
    elif ! cmp --silent "${SOURCE}" "${CACHED}"; then
      ((++VERIFY_ERROR))
    fi
  fi
  if ((VERIFY_ERROR != 0)); then
    echo "${PROGRAM}: ${NAME}: Source/GCS mismatch"
    ((++VERIFY_ERRORS))
  fi
  echo ''
done
if ((VERIFY_ERRORS != 0)); then
  echo '[ Verification error(s) ]'
  exit 1
fi

echo '[ SUCCESS ]'
exit 0
