#!/usr/bin/env bash
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

set -eu

source "$(dirname "$0")/lib/init.sh"
source module lib/io.sh

if [[ "${CHECK_GENERATED_CODE_HASH}" != "yes" ]]; then
  echo "Skipping the generated code hash check as it is disabled for this build."
  exit 0
fi

# defines array GOLDEN_FILE_MD5_HASHES=("hash filepath")
source "${PROJECT_ROOT}/ci/etc/generator-golden-md5-hashes.sh"

cd "${PROJECT_ROOT}"
readarray -d '' current_golden_files < <(find generator/integration_tests/golden -name '*.gcpcxx.*' -print0 | sort -z)

if [ "${#current_golden_files[@]}" -gt "${#GOLDEN_FILE_MD5_HASHES[@]}" ]; then
  io::log_red "New golden files have been added."
  io::log_yellow "Run 'ci/kokoro/docker/build.sh generate-libraries' to update."
  exit 1
fi

for i in "${!current_golden_files[@]}"; do
  read -r -a tuple <<<"${GOLDEN_FILE_MD5_HASHES[i]}"
  md5_hash=${tuple[0]}
  golden_file=${tuple[1]}

  current_md5_hash_output=$(md5sum "${current_golden_files[i]}")
  read -r -a current_tuple <<<"${current_md5_hash_output}"
  current_md5_hash=${current_tuple[0]}
  current_golden_file=${current_tuple[1]}

  if [ "${current_golden_file}" == "${golden_file}" ]; then
    if [ "${current_md5_hash}" != "${md5_hash}" ]; then
      io::log_red "${current_golden_file} md5 hash has changed."
      io::log_yellow "Run 'ci/kokoro/docker/build.sh generate-libraries' to update."
      exit 1
    fi
  else
    io::log_red "Unexpected file ${current_golden_file}"
    io::log_yellow "Run 'ci/kokoro/docker/build.sh generate-libraries' to update."
    exit 1
  fi
done

exit 0
