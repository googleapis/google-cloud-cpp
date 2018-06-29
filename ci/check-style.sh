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

find google/cloud -name '*.h' -o -name '*.cc' -print0 \
    | xargs -0 sed -i 's/grpc::\([A-Z][A-Z_][A-Z_]*\)/grpc::StatusCode::\1/g'

# Apply clang-format(1) to fix whitespace and other formatting rules.
# The version of clang-format is important, different versions have slightly
# different formatting output (sigh).
find . \( -path ./.git -prune -o -path ./third_party -prune \
          -o -path './cmake-build-*' -o -path ./build-output -prune \
          -o -name '*.pb.h' -prune -o -name '*.pb.cc' -prune \) \
     -o \( -name '*.cc' -o -name '*.h' \) -print0 \
     | xargs -0 clang-format -i

# Replace any #include for grpc++/grpc++.h with grpcpp/grpcpp.h, and in general,
# any include of grpc++/ files with grpcpp/.  The paths with grpc++ are
# obsoleted by the gRPC team, so we should not use them in our code.
find . \( -path ./.git -prune -o -path ./third_party -prune \
          -o -path './cmake-build-*' -o -path ./build-output -prune \
          -o -name '*.pb.h' -prune -o -name '*.pb.cc' -prune \) \
     -o \( -name '*.cc' -o -name '*.h' \) -print0 \
     |  xargs -0 sed -i 's;#include <grpc\\+\\+/grpc\+\+.h>;#include <grpcpp/grpcpp.h>;'
find . \( -path ./.git -prune -o -path ./third_party -prune \
          -o -path './cmake-build-*' -o -path ./build-output -prune \
          -o -name '*.pb.h' -prune -o -name '*.pb.cc' -prune \) \
     -o \( -name '*.cc' -o -name '*.h' \) -print0 \
     |  xargs -0 sed -i 's;#include <grpc\\+\\+/;#include <grpcpp/;'

# Report any differences created by running clang-format.
git diff --ignore-submodules=all --color --exit-code .
