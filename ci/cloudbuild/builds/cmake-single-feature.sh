#!/bin/bash
#
# Copyright 2022 Google LLC
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

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/lib/io.sh
source module ci/cloudbuild/builds/lib/features.sh

mapfile -t features < <(features::list_full)
for feature in "${features[@]}"; do
  io::log_h2 cmake -S . -B cmake-out/test-only-"${feature}" \
    -DGOOGLE_CLOUD_CPP_ENABLE="${feature}"
  cmake -S . -B cmake-out/test-only-"${feature}" \
    -DGOOGLE_CLOUD_CPP_ENABLE="${feature}"
done
