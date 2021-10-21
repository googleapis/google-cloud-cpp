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

set -eu

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/etc/quickstart-config.sh
source module ci/lib/io.sh

export CC=gcc
export CXX=g++

mapfile -t args < <(bazel::common_args)
for lib in $(quickstart::libraries); do
  io::log_h2 "Running Bazel quickstart for ${lib}"
  mapfile -t run_args < <(quickstart::arguments "${lib}")
  env -C "${PROJECT_ROOT}/google/cloud/${lib}/quickstart" \
    bazel run "${args[@]}" :quickstart -- "${run_args[@]}"
done
