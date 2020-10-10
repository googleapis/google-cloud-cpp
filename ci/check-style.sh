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

source "$(dirname "$0")/lib/init.sh"
source module lib/io.sh

if [[ "${CHECK_STYLE}" != "yes" ]]; then
  echo "Skipping code style check as it is disabled for this build."
  exit 0
fi

# This script must run from the top-level google-cloud-cpp directory.
cd "${PROJECT_ROOT}"

# This controls the output format from bash's `time` command, which we use
# below to time blocks of the script. A newline is automatically included.
readonly TIMEFORMAT="... %R seconds"

problems=""
printf "%-30s" "Running check-include-guards:"
time {
  if ! git ls-files -z | grep -zE '\.h$' |
    xargs -0 awk -f "ci/check-include-guards.gawk"; then
    problems="${problems} include-guards"
  fi
}

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
printf "%-30s" "Running cmake-format:"
time {
  git ls-files -z | grep -zE '((^|/)CMakeLists\.txt|\.cmake)$' |
    xargs -P "${NCPU}" -n 1 -0 cmake-format -i
}

# TODO(#4501) - this fixup can be removed if #include <absl/...> works
# Apply transformations to fix errors on MSVC+x86. See the bug for a detailed
# explanation as to why this is needed:
#   https://github.com/googleapis/google-cloud-cpp/issues/4501
# This should run before clang-format because it might alter the order of any
# includes.
printf "%-30s" "Running Abseil header fixes:"
time {
  git ls-files -z |
    grep -zv 'google/cloud/internal/absl_.*quiet.h$' |
    grep -zE '\.(h|cc)$' |
    while IFS= read -r -d $'\0' file; do
      sed -e 's;#include "absl/strings/str_\(cat\|replace\|join\).h";#include "google/cloud/internal/absl_str_\1_quiet.h";' \
        "${file}" >"${file}.tmp"
      replace_original_if_changed "${file}" "${file}.tmp"
    done
}

# Apply clang-format(1) to fix whitespace and other formatting rules.
# The version of clang-format is important, different versions have slightly
# different formatting output (sigh).
printf "%-30s" "Running clang-format:"
time {
  git ls-files -z | grep -zE '\.(cc|h)$' |
    xargs -P "${NCPU}" -n 50 -0 clang-format -i
}

# Apply buildifier to fix the BUILD and .bzl formatting rules.
#    https://github.com/bazelbuild/buildtools/tree/master/buildifier
printf "%-30s" "Running buildifier:"
time {
  git ls-files -z | grep -zE '\.(BUILD|bzl)$' | xargs -0 buildifier -mode=fix
  git ls-files -z | grep -zE '(^|/)(BUILD|WORKSPACE)$' |
    xargs -0 buildifier -mode=fix
}

# Apply psf/black to format Python files.
#    https://pypi.org/project/black/
printf "%-30s" "Running black:"
time {
  git ls-files -z | grep -z '\.py$' | xargs -0 python3 -m black --quiet
}

# Apply shfmt to format all shell scripts
printf "%-30s" "Running shfmt:"
time {
  git ls-files -z | grep -z '\.sh$' | xargs -0 shfmt -w -i 2
}

# Apply shellcheck(1) to emit warnings for common scripting mistakes.
printf "%-30s" "Running shellcheck:"
time {
  if ! git ls-files -z | grep -z '\.sh$' |
    xargs -0 shellcheck \
      --exclude=SC1090 \
      --exclude=SC1091 \
      --exclude=SC2034 \
      --exclude=SC2153 \
      --exclude=SC2181; then
    problems="${problems} shellcheck"
  fi
}

printf "%-30s" "Running cspell:"
time {
  if ! git ls-files -z | grep -zE '\.(cc|h)$' |
    xargs -P "${NCPU}" -n 50 -0 cspell --no-summary -c ci/cspell.json; then
    problems="${problems} cspell"
  fi
}

# Apply several transformations that cannot be enforced by clang-format:
#     - Replace any #include for grpc++/* with grpcpp/*. The paths with grpc++
#       are obsoleted by the gRPC team, so we should not use them in our code.
#     - Replace grpc::<BLAH> with grpc::StatusCode::<BLAH>, the aliases in the
#       `grpc::` namespace do not exist inside google.
printf "%-30s" "Running include fixes:"
time {
  git ls-files -z | grep -zE '\.(cc|h)$' |
    while IFS= read -r -d $'\0' file; do
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
}

# Apply transformations to fix whitespace formatting in files not handled by
# clang-format(1) above.  For now we simply remove trailing blanks.  Note that
# we do not expand TABs (they currently only appear in Makefiles and Makefile
# snippets).
printf "%-30s" "Running whitespace fixes:"
time {
  git ls-files -z | grep -zv '\.gz$' |
    while IFS= read -r -d $'\0' file; do
      sed -e 's/[[:blank:]][[:blank:]]*$//' \
        "${file}" >"${file}.tmp"
      replace_original_if_changed "${file}" "${file}.tmp"
    done
}

# Report any differences created by running the formatting tools.
if ! git diff --ignore-submodules=all --color --exit-code .; then
  problems="${problems} formatting"
fi

# Exit with an error IFF we are running as part of a CI build. Otherwise, a
# human probably invoked this script in which case we'll just leave the
# formatted files in their local git repo so they can diff them and commit the
# correctly formatted files.
if [[ -n "${problems}" ]]; then
  io::log_red "Detected style problems (${problems:1})"
  if [[ "${RUNNING_CI}" != "no" ]]; then
    exit 1
  fi
fi
