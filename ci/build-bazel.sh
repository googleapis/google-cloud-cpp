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

if [ "${TERM:-}" = "dumb" ]; then
  readonly COLOR_RED=""
  readonly COLOR_GREEN=""
  readonly COLOR_YELLOW=""
  readonly COLOR_RESET=""
else
  readonly COLOR_RED=$(tput setaf 1)
  readonly COLOR_GREEN=$(tput setaf 2)
  readonly COLOR_YELLOW=$(tput setaf 3)
  readonly COLOR_RESET=$(tput sgr0)
fi

export PATH=$PATH:$HOME/bin

# We cannot simply use //...:all because when submodules are checked out that
# includes the BUILD files for gRPC, protobuf, etc.
# TODO(#496) - just use //google/...:all when it becomes available.
for subdir in bigtable firestore google storage; do
  bazel --batch build "//${subdir}/...:all"
  bazel --batch test  "//${subdir}/...:all"
done

if [ "${TEST_BAZEL_AS_DEPENDENCY:-}" = "yes" ]; then
  echo
  echo "${COLOR_YELLOW}Testing Bazel files as dependency${COLOR_RESET}"
  (cd ci/test-install && bazel --batch build //...:all)
fi
