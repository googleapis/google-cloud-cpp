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
#
# This script saves a cache tarball to GCS, and restores one from GCS. It is
# intended to be called from a build step in the `cloudbuild.yaml` file. It
# executes in the context of build. Typically the "restore cache" step will be
# called in one step, followed by the actual build of the code, followed by the
# "save cache" step.
#
# Usage: cache.sh <save|restore>
#
#   Options:
#     --bucket_url=<url>    URL to the GCS bucket where the cache should be saved
#     --key=<key>           The primary key to use when caching
#     --fallback_key=<key>  If the primary key isn't found when restoring, try this key
#     --path=<path>         A path to be backed up. May be specified multiple times.
#     -h|--help             Print this help message

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh

function print_usage() {
  # Extracts the usage from the file comment starting at line 17.
  sed -n '17,/^$/s/^# \?//p' "${PROGRAM_PATH}"
}

io::log "Running:"
printf "env -C '%s' %s" "$(pwd)" "$0"
printf " %q" "$@"
printf "\n"

# Use getopt to parse and normalize all the args.
PARSED="$(getopt -a \
  --options="h" \
  --longoptions="bucket_url:,key:,fallback_key:,path:,help" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

BUCKET_URL=""
KEY=""
FALLBACK_KEY=""
PATHS=()
while true; do
  case "$1" in
  --bucket_url)
    BUCKET_URL="$2"
    shift 2
    ;;
  --key)
    KEY="$2"
    shift 2
    ;;
  --fallback_key)
    FALLBACK_KEY="$2"
    shift 2
    ;;
  --path)
    PATHS+=("$2")
    shift 2
    ;;
  -h | --help)
    print_usage
    exit 0
    ;;
  --)
    shift
    break
    ;;
  esac
done
readonly BUCKET_URL
readonly KEY
readonly FALLBACK_KEY
readonly PATHS

if [[ $# -eq 0 ]]; then
  echo "Missing action: save|restore"
  print_usage
  exit 1
elif [[ -z "${BUCKET_URL}" ]]; then
  echo "Missing --bucket_url="
  print_usage
  exit 1
elif [[ -z "${KEY}" ]]; then
  echo "Missing --key="
  print_usage
  exit 1
fi

readonly PRIMARY_CACHE_URL="${BUCKET_URL}/${KEY}/cache.tar.gz"

function save_cache() {
  # Filters PATHS to only those that exist.
  paths=()
  for p in "${PATHS[@]}"; do
    test -r "${p}" && paths+=("${p}")
  done
  if ((${#paths[@]} == 0)); then
    io::log "No paths to cache found."
    return 0
  fi
  io::log "Saving ( ${paths[*]} ) to ${PRIMARY_CACHE_URL}"
  du -sh "${paths[@]}"
  # We use a temp file here so gsutil can retry failed uploads. See #6508.
  tmpd="$(mktemp -d)"
  tmpf="${tmpd}/cache.tar.gz"
  tar -czf "${tmpf}" "${paths[@]}"
  gsutil cp "${tmpf}" "${PRIMARY_CACHE_URL}"
  gsutil stat "${PRIMARY_CACHE_URL}"
  rm "${tmpf}"
  rmdir "${tmpd}"
}

function restore_cache() {
  local urls=("${PRIMARY_CACHE_URL}")
  if [[ -n "${FALLBACK_KEY}" ]]; then
    urls+=("${BUCKET_URL}/${FALLBACK_KEY}/cache.tar.gz")
  fi
  for url in "${urls[@]}"; do
    if gsutil stat "${url}"; then
      io::log "Fetching cache url ${url}"
      gsutil cp "${url}" - | tar -zxf - || continue
      break
    fi
  done
  return 0
}

io::log "====> ${PROGRAM_NAME}: $*"
readonly TIMEFORMAT="==> 🕑 ${PROGRAM_NAME} completed in %R seconds"
time {
  case "$1" in
  save)
    save_cache
    ;;
  restore)
    restore_cache
    ;;
  *)
    print_usage
    ;;
  esac
}
