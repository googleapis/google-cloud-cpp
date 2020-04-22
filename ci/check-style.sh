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

if [[ "${CHECK_STYLE}" != "yes" ]]; then
  echo "Skipping code style check as it is disabled for this build."
  exit 0
fi

# This script assumes it is running the top-level google-cloud-cpp directory.

readonly BINDIR="$(dirname "$0")"
source "${BINDIR}/colors.sh"

problems=""
if ! find google/cloud -name '*.h' -print0 |
  xargs -0 awk -f "${BINDIR}/check-include-guards.gawk"; then
  problems="${problems} include-guards"
fi

replace_original_if_changed() {
  if [[ $# != 2 ]]; then
    return 1
  fi

  local original="$1"
  local reformatted="$2"

  if cmp -s "${original}" "${reformatted}"; then
    rm -f "${reformatted}"
  else
    chmod --reference="${original}" "${reformatted}"
    mv -f "${reformatted}" "${original}"
  fi
}

# Apply cmake_format to all the CMake list files.
#     https://github.com/cheshirekow/cmake_format
git ls-files | grep -P '(CMakeLists\.txt|\.cmake)$' | xargs cmake-format -i

# Apply clang-format(1) to fix whitespace and other formatting rules.
# The version of clang-format is important, different versions have slightly
# different formatting output (sigh).
git ls-files | grep -P '\.(cc|h)$' | xargs clang-format -i

# Apply buildifier to fix the BUILD and .bzl formatting rules.
#    https://github.com/bazelbuild/buildtools/tree/master/buildifier
git ls-files | grep -P '\.(BUILD|bzl)$' | xargs buildifier -mode=fix

# Apply psf/black to format Python files.
#    https://github.com/bazelbuild/buildtools/tree/master/buildifier
git ls-files | grep '\.py$' | xargs python3 -m black

# Apply shfmt to format all shell scripts
git ls-files | grep '\.sh$' | xargs shfmt -w -i 2

# Apply shellcheck(1) to emit warnings for common scripting mistakes.
if ! git ls-files | grep '\.sh$' |
  xargs shellcheck \
    --exclude=SC1090 \
    --exclude=SC2034 \
    --exclude=SC2153 \
    --exclude=SC2181; then
  problems="${problems} shellcheck"
fi

# Apply several transformations that cannot be enforced by clang-format:
#     - Replace any #include for grpc++/* with grpcpp/*. The paths with grpc++
#       are obsoleted by the gRPC team, so we should not use them in our code.
#     - Replace grpc::<BLAH> with grpc::StatusCode::<BLAH>, the aliases in the
#       `grpc::` namespace do not exist inside google.
git ls-files | grep -P '\.(cc|h)$' |
  while IFS= read -r file; do
    # We used to run run `sed -i` to apply these changes, but that touches the
    # files even if there are no changes applied, forcing a rebuild each time.
    # So we first apply the change to a temporary file, and replace the original
    # only if something changed.
    sed -e 's/grpc::\([A-Z][A-Z_][A-Z_]*\)/grpc::StatusCode::\1/g' \
      -e 's;#include <grpc\\+\\+/grpc\+\+.h>;#include <grpcpp/grpcpp.h>;' \
      -e 's;#include <grpc\\+\\+/;#include <grpcpp/;' \
      "${file}" >"${file}.tmp"
    replace_original_if_changed "${file}" "${file}.tmp"
  done

# Apply transformations to fix whitespace formatting in files not handled by
# clang-format(1) above.  For now we simply remove trailing blanks.  Note that
# we do not expand TABs (they currently only appear in Makefiles and Makefile
# snippets).
git ls-files | grep -v '\.gz$' |
  while IFS= read -r file; do
    sed -e 's/[[:blank:]][[:blank:]]*$//' \
      "${file}" >"${file}.tmp"
    replace_original_if_changed "${file}" "${file}.tmp"
  done

# Report any differences created by running the formatting tools. Report any
# differences created by running the formatting tools.
if ! git diff --ignore-submodules=all --color --exit-code .; then
  problems="${problems} formatting"
fi

# Exit with an error IFF we are running as part of a CI build. Otherwise, a
# human probably invoked this script in which case we'll just leave the
# formatted files in their local git repo so they can diff them and commit the
# correctly formatted files.
if [[ -n "${problems}" ]]; then
  log_red "Detected style problems (${problems:1})"
  if [[ "${RUNNING_CI}" != "no" ]]; then
    exit 1
  fi
fi
