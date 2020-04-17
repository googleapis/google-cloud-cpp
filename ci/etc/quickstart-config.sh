#!/usr/bin/env bash
# Copyright 2020 Google LLC
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

quickstart_libraries() {
  echo "bigtable"
  echo "storage"
}

quickstart_arguments() {
  local -r library="$1"
  case "${library}" in
  "bigtable")
    echo "${GOOGLE_CLOUD_PROJECT}"
    echo "${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID}"
    echo "quickstart"
    return 0
    ;;
  "storage")
    echo "${GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
    return 0
    ;;
  *)
    echo "Unknown argument list for ${library}'s quickstart"
    ;;
  esac
  return 1
}
