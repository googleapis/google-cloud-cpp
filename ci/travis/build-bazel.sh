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

readonly BINDIR="$(dirname "$0")"
source "${BINDIR}/../colors.sh"

export PATH=$PATH:$HOME/bin

if [ "${TEST_BAZEL_AS_DEPENDENCY:-}" = "yes" ]; then
  echo
  echo "${COLOR_YELLOW}Testing Bazel files as dependency${COLOR_RESET}"
  (cd ci/test-install && bazel --batch build //...:all)
else
  # We cannot simply use //...:all because when submodules are checked out that
  # includes the BUILD files for gRPC, protobuf, etc.
  bazel --batch build \
      --test_output=errors \
      --action_env="GTEST_COLOR=1" \
      "//google/cloud/...:all"
  bazel --batch test \
      --test_output=errors \
      --action_env="GTEST_COLOR=1" \
      "//google/cloud/...:all"
fi
