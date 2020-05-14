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

# This bash library should be the first library sourced by all of our bash
# scripts. This library does some basic initialization, such as setting some
# global variables that are commonly needed in a lot of places, including
# needed by other libraries. Typically, a bash script will start as follows
#
#   set -eu  # We commonly do this, but it's optional
#   source "$(dirname "$0")/../../lib/init.sh"  # The path to this library
#   source module lib/io.sh
#   source module lib/foo.sh
#
# Further more, a bash library may source other bash libraries directly by
# using a `source module lib/foo.sh` line. I.e., there's no need for a bash
# library to source init -- it may assume that the binary that called the
# library has already called init.sh

# Make our include guard clean against set -o nounset.
test -n "${CI_LIB_INIT_SH__:-}" || declare -i CI_LIB_INIT_SH__=0
if ((CI_LIB_INIT_SH__++ != 0)); then
  return 0
fi # include guard

function init::repo_root() {
  local dir="$1"
  while [[ ! -f "${dir}/WORKSPACE" ]]; do
    dir="$(dirname "$dir")"
  done
  echo "${dir}"
}

PROGRAM_PATH="$(realpath "$0")"
PROGRAM_NAME="$(basename "${PROGRAM_PATH}")"
PROGRAM_DIR="$(dirname "${PROGRAM_PATH}")"
PROJECT_ROOT="$(init::repo_root "${PROGRAM_DIR}")"

# Sets the path to the `module` library in PATH so that it can be found when
# callers use `source module <args>`
if [[ ! "$PATH" =~ ${PROJECT_ROOT}/ci/lib ]]; then
  # Changing PATH invalidates the Bazel cache, while this script runs at most
  # once when called by a single script, it might be loaded again by a child
  # script.
  PATH="${PROJECT_ROOT}/ci/lib:${PATH}"
fi

# Sets a search path array for the "modules" that can be sourced. When a
# caller writes `source module lib/foo.sh`, this path/array will be searched in
# order for the caller-provided 'lib/foo.sh'. The first one found will be used.
MODULE_SEARCH_PATH=(
  "${PROGRAM_DIR}"
  "${PROJECT_ROOT}/ci"
)
