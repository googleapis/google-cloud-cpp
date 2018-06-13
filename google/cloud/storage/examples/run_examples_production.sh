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

# This script should is called from the build directory, and it finds other
# scripts in the source directory using its own path.
readonly BINDIR="$(dirname $0)"
source "${BINDIR}/../../../../ci/colors.sh"

function run_all_bucket_examples {
  local bucket_name = $1
  shift

  echo
  echo "Running bucket get-metadata example"
  ./storage_bucket_samples get-metadata "${bucket_name}"

  echo
  echo "Running bucket insert-object example"
  ./storage_bucket_samples insert-object "${bucket_name}"
}

# Run the examples against the production environment:
echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET} Running Google Cloud Storage Examples"
for example in bucket; do
  log="$(mktemp --tmpdir "storage_examples_${example}.XXXXXXXXXX.log")"
  echo "${COLOR_GREEN}[ RUN      ]${COLOR_RESET} ${example}"
  # The CI environment must provide BUCKET_NAME.
  run_all_${example}_examples "${BUCKET_NAME}" >"${log}" 2>&1 </dev/null
  if [ $? = 0 ]; then
    echo "${COLOR_GREEN}[       OK ]${COLOR_RESET} ${example} examples"
  else
    echo   "${COLOR_RED}[    ERROR ]${COLOR_RESET} ${example} examples"
    echo
    echo "================ [begin ${log}] ================"
    cat "${log}"
    echo "================ [end ${log}] ================"
  fi
done
echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET} Google Cloud Storage Examples Finished"
