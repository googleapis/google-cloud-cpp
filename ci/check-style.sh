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

find google/cloud storage -name '*.h' -print0 \
  | xargs -0 awk 'BEGINFILE {
    # The guard must begin with the name of the project.
    guard_prefix="GOOGLE_CLOUD_CPP_"
    # The guard must end with "_"
    guard_suffix="_"
    # The guard name is the filename (including path from the root of the
    # project), with "/" and "." characters replaced with "_", and all
    # characters converted to uppercase:
    guard_body=toupper(FILENAME)
    gsub("[/\\.]", "_", guard_body)
    guard=toupper(guard_prefix guard_body guard_suffix)
    matches=0
  }
  /^#ifndef / {
    # Ignore lines that do not look like guards at all.
    if ($0 !~ "_H_$") { next; }
    if (index($0, guard) == 9) {
      matches++;
    } else {
     printf("%s:\nexpected: #ifndef %s\n   found: %s\n", FILENAME, guard, $0);
    }
  }
  /^#define / {
    # Ignore lines that do not look like guards at all.
    if ($0 !~ "_H_$") { next; }
    if (index($0, guard) == 9) {
      matches++;
    } else {
      printf("%s:\nexpected: #define %s\n   found: %s\n", FILENAME, guard, $0);
    }
  }
  /^#endif / {
    # Ignore lines that do not look like guards at all.
    if ($0 !~ "_H_$") { next; }
    if (index($0, "// " guard) == 9) {
      matches++;
    } else {
      printf("%s:\nexpected: #endif  // %s\n   found: %s\n",
             FILENAME, guard, $0);
    }
  }
  END {
    if (matches != 3) {
      printf("%s has invalid include guards\n", FILENAME);
      exit(1);
    }
  }'

# This script assumes it is running the top-level google-cloud-cpp directory.

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
