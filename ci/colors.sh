#!/usr/bin/env bash
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

# Prefer the log_* functions, such as log_yellow, instead of directly using
# these variables. The functions don't require the caller to remember to reset
# the terminal.
if [[ -z "${COLOR_RESET+x}" ]]; then
  if command -v tput >/dev/null && [[ -n "${TERM:-}" ]]; then
    readonly COLOR_RED="$(tput setaf 1)"
    readonly COLOR_GREEN="$(tput setaf 2)"
    readonly COLOR_YELLOW="$(tput setaf 3)"
    readonly COLOR_RESET="$(tput sgr0)"
  else
    readonly COLOR_RED=""
    readonly COLOR_GREEN=""
    readonly COLOR_YELLOW=""
    readonly COLOR_RESET=""
  fi
fi

# Logs a message using the given color. The first argument must be one of the
# COLOR_* variables defined above, such as "${COLOR_YELLOW}". The remaining
# arguments will be logged in the given color. The log message will also have
# an RFC-3339 timestamp prepended (in UTC).
function log_color_impl() {
  local color="$1"
  shift
  local timestamp
  timestamp="$(date -u "+%Y-%m-%dT%H:%M:%SZ")"
  echo "${color}${timestamp}:" "$@" "${COLOR_RESET}"
}

# Logs the given message with normal coloring and a timestamp.
function log_normal() {
  log_color_impl "${COLOR_RESET}" "$@"
}

# Logs the given message in green with a timestamp.
function log_green() {
  log_color_impl "${COLOR_GREEN}" "$@"
}

# Logs the given message in yellow with a timestamp.
function log_yellow() {
  log_color_impl "${COLOR_YELLOW}" "$@"
}

# Logs the given message in red with a timestamp.
function log_red() {
  log_color_impl "${COLOR_RED}" "$@"
}
