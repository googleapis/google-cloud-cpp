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

# TODO(#3604) - figure out how to run pass arguments safely
# We would like to declare an associate array of arrays, but not sure how to
# do that. This works because all the variables below are "words", without
# spaces, but it would be nice to have something more robust.
declare -A -r quickstart_args=(
  ["storage"]="${GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
  ["bigtable"]="${GOOGLE_CLOUD_PROJECT} ${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID} quickstart"
)
