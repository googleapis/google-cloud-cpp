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
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/colors.sh"

################################################
# Run all Bucket examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_bucket_examples() {
  local bucket_name=$1
  shift

  # The list of commands in the storage_bucket_samples program that we will
  # test. Currently get-metadata assumes that $bucket_name is already created.
  readonly BUCKET_EXAMPLES_COMMANDS="get-metadata insert-object"

  if [ ! -x storage_bucket_samples ]; then
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " storage_bucket_samples is not compiled"
    return
  fi
  local object_name="object-$(date +%s)"
  for example in get-bucket-metadata insert-object; do
    log="$(mktemp --tmpdir "storage_bucket_samples_${example}.XXXXXXXXXX.log")"
    echo    "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
        "storage_bucket_samples ${example} running"
    case ${example} in
        insert-object)
            parameters="${object_name} a-short-string-to-put-in-the-object"
            ;;
        *)
            parameters=""
            ;;
    esac
    ./storage_bucket_samples \
        ${example} "${bucket_name}" ${parameters} >"${log}" 2>&1 </dev/null
    if [ $? = 0 ]; then
      echo  "${COLOR_GREEN}[       OK ]${COLOR_RESET} ${example}"
      continue
    else
      echo    "${COLOR_RED}[    ERROR ]${COLOR_RESET} ${example}"
      echo
      echo "================ [begin ${log}] ================"
      cat "${log}"
      echo "================ [end ${log}] ================"
    fi
    /bin/rm -f "${log}"
  done

  log="$(mktemp --tmpdir "storage_bucket_samples_${example}.XXXXXXXXXX.log")"
  echo "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
      "storage_bucket_samples with no command running"
  ./storage_bucket_samples >"${log}" 2>&1 </dev/null
  # Note the inverted test, this is supposed to exit with 1.
  if [ $? != 0 ]; then
    echo "${COLOR_GREEN}[       OK ]${COLOR_RESET} (no command)"
  else
    echo   "${COLOR_RED}[    ERROR ]${COLOR_RESET} (no command)"
    echo
    echo "================ [begin ${log}] ================"
    cat "${log}"
    echo "================ [end ${log}] ================"
  fi
  /bin/rm -f "${log}"
}

################################################
# Run all the examples.
# Globals:
#   BUCKET_NAME: the name of the bucket to use in the examples.
#   COLOR_*: colorize output messages, defined in colors.sh
# Arguments:
#   None
# Returns:
#   None
################################################
run_all_storage_examples() {
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET}" \
      " Running Google Cloud Storage Examples"
  set +e
  run_all_bucket_examples "${BUCKET_NAME}"
  set -e
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET}" \
      " Google Cloud Storage Examples Finished"
}
