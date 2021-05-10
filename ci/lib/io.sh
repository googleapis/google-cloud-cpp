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

# This bash library includes various I/O funtions, including logging functions.
#
# Example:
#
#   source module /ci/lib/io.sh
#   io::log_yellow "Hello world"

# Make our include guard clean against set -o nounset.
test -n "${CI_LIB_IO_SH__:-}" || declare -i CI_LIB_IO_SH__=0
if ((CI_LIB_IO_SH__++ != 0)); then
  return 0
fi # include guard

# Callers may use these IO_* variables directly, but should prefer to use the
# logging functions below instead. For example, prefer `io::log_green "..."`
# over `echo "${IO_COLOR_GREEN}...${IO_RESET}"`.
if [ -t 0 ] && command -v tput >/dev/null; then
  readonly IO_BOLD="$(tput bold)"
  readonly IO_COLOR_RED="$(tput setaf 1)"
  readonly IO_COLOR_GREEN="$(tput setaf 2)"
  readonly IO_COLOR_YELLOW="$(tput setaf 3)"
  readonly IO_RESET="$(tput sgr0)"
else
  readonly IO_BOLD=""
  readonly IO_COLOR_RED=""
  readonly IO_COLOR_GREEN=""
  readonly IO_COLOR_YELLOW=""
  readonly IO_RESET=""
fi

# Logs a message using the given terminal capability. The first argument
# must be one of the IO_* variables defined above, such as "${IO_COLOR_RED}".
# The remaining arguments will be logged using the given capability. The
# log message will also have an RFC-3339 timestamp prepended (in UTC).
function io::internal::log_impl() {
  local termcap="$1"
  shift
  local timestamp
  timestamp="$(date -u "+%Y-%m-%dT%H:%M:%SZ")"
  echo "${termcap}${timestamp}: $*${IO_RESET}"
}

# Logs the given message with normal coloring and a timestamp.
function io::log() {
  io::internal::log_impl "${IO_RESET}" "$@"
}

# Logs the given message in green with a timestamp.
function io::log_green() {
  io::internal::log_impl "${IO_COLOR_GREEN}" "$@"
}

# Logs the given message in yellow with a timestamp.
function io::log_yellow() {
  io::internal::log_impl "${IO_COLOR_YELLOW}" "$@"
}

# Logs the given message in red with a timestamp.
function io::log_red() {
  io::internal::log_impl "${IO_COLOR_RED}" "$@"
}

# Logs the arguments, in bold with a timestamp, like they were a command
# executed under "set -x" in the shell (i.e., with a ${PS4} prefix).
function io::log_cmdline() {
  io::internal::log_impl "${IO_BOLD}" "${PS4}$*"
}

# Logs an "H1" heading. This looks like a blank line, followed by the message
# in a double-lined box.
#
# ========================================
# |   This is an example of io::log_h1   |
# ========================================
function io::log_h1() {
  local msg="|   $*   |"
  local line
  line="$(printf -- "=%.0s" $(seq 1 ${#msg}))"
  printf "\n%s\n%s\n%s\n" "${line}" "${msg}" "${line}"
}

# Logs an "H2" heading. Same as H1, but uses a single-lined box.
#
# ----------------------------------------
# |   This is an example of io::log_h2   |
# ----------------------------------------
function io::log_h2() {
  local msg="|   $*   |"
  local line
  line="$(printf -- "-%.0s" $(seq 1 ${#msg}))"
  printf "\n%s\n%s\n%s\n" "${line}" "${msg}" "${line}"
}
