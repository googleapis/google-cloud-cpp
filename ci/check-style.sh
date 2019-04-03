#!/usr/bin/env bash
#
# Copyright 2017 Google Inc.
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

if [ "${CHECK_STYLE}" != "yes" ]; then
  echo "Skipping code style check as it is disabled for this build."
  exit 0
fi

# This script assumes it is running the top-level google-cloud-cpp directory.

readonly BINDIR="$(dirname $0)"

find google/cloud -name '*.h' -print0 \
  | xargs -0 awk -f ${BINDIR}/check-include-guards.gawk

# Apply cmake_format to all the CMake list files.
#     https://github.com/cheshirekow/cmake_format
find . \( -path ./.git \
          -o -path ./third_party \
          -o -path './cmake-build-*' \
          -o -path ./build-output \
          -o -path ./.build \
          -o -path ./_build \
       \) -prune \
       -o \( -name 'CMakeLists.txt' \
             -o -name '*.cmake' \
          \) -print0 \
     | xargs -0 cmake-format -i

# Apply clang-format(1) to fix whitespace and other formatting rules.
# The version of clang-format is important, different versions have slightly
# different formatting output (sigh).
find google/cloud \( -name '*.cc' -o -name '*.h' \) -print0 \
     | xargs -0 clang-format -i

# Apply several transformations that cannot be enforced by clang-format:
#     - Replace any #include for grpc++/* with grpcpp/*. The paths with grpc++
#       are obsoleted by the gRPC team, so we should not use them in our code.
#     - Replace grpc::<BLAH> with grpc::StatusCode::<BLAH>, the aliases in the
#       `grpc::` namespace do not exist inside google.
for file in $(find google/cloud -name '*.h' -o -name '*.cc' -print); do
  # We used to run run `sed -i` to apply these changes, but that touches the
  # files even if there are no changes applied, forcing a rebuild each time.
  # So we first apply the change to a temporary file, and replace the original
  # only if something changed.
  sed -e 's/grpc::\([A-Z][A-Z_][A-Z_]*\)/grpc::StatusCode::\1/g' \
      -e 's;#include <grpc\\+\\+/grpc\+\+.h>;#include <grpcpp/grpcpp.h>;' \
      -e 's;#include <grpc\\+\\+/;#include <grpcpp/;' \
      "${file}" > "${file}.tmp"
  if cmp "${file}" "${file}.tmp"; then
      rm -f "${file}.tmp"
  else
      mv -f "${file}.tmp" "${file}"
  fi
done

# Apply buildifier to fix the BUILD and .bzl formatting rules.
#    https://github.com/bazelbuild/buildtools/tree/master/buildifier
find . \( -path ./.git \
          -o -path ./third_party \
          -o -path './cmake-build-*' \
          -o -path ./build-output \
          -o -path ./.build \
          -o -path ./_build \
       \) -prune \
     -o \( -name BUILD \
           -o -name '*.bzl' \
        \) -print0 \
     | xargs -0 buildifier -mode=fix

# Report any differences created by running clang-format.
git diff --ignore-submodules=all --color --exit-code .
