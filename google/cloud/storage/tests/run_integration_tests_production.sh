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

# The CI environment must provide PROJECT_ID and BUCKET_NAME.

echo
echo "Running GCS Bucket APIs integration tests."
./bucket_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

echo
echo "Running GCS Object APIs integration tests."
./object_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"
