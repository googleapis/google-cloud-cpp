#!/usr/bin/env bash
#
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_WINDOWS_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_WINDOWS_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_WINDOWS_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

function google_time() {
  curl -sI google.com | tr -d '\r' | sed -n 's/Date: \(.*\)/\1/p'
}

mapfile -t CI_CLOUDBUILD_BUILDS_LIB_WINDOWS_SH_SYSTEM_INFO < <(systeminfo | tr '\t' ' ')

function os::name() {
  printf "%s\n" "${CI_CLOUDBUILD_BUILDS_LIB_WINDOWS_SH_SYSTEM_INFO[@]}" | sed -n 's/^OS Name: \(.*\)/\1/p'
}

function os::version() {
  printf "%s\n" "${CI_CLOUDBUILD_BUILDS_LIB_WINDOWS_SH_SYSTEM_INFO[@]}" | sed -n 's/^OS Version: \(.*\)/\1/p'
}

function os::mem() {
  printf "%s\n" "${CI_CLOUDBUILD_BUILDS_LIB_WINDOWS_SH_SYSTEM_INFO[@]}" | sed -n 's/Virtual Memory: Available: \(.*\)/\1/p'
}

function os::cpus() {
  nproc
}

function os::prefetch() {
  echo "@remotejdk11_win//:jdk"
}

io::log_h1 "Machine Info"
printf "%10s %s\n" "host:" "$(date -u --rfc-3339=seconds)"
printf "%10s %s\n" "google:" "$(date -ud "$(google_time)" --rfc-3339=seconds)"
printf "%10s %s\n" "kernel:" "$(uname -v)"
printf "%10s %s\n" "os name:" "$(os::name)"
printf "%10s %s\n" "os version:" "$(os::version)"
printf "%10s %s\n" "nproc:" "$(os::cpus)"
printf "%10s %s\n" "mem:" "$(os::mem)"
printf "%10s %s\n" "term:" "${TERM-}"
printf "%10s %s\n" "bash:" "$(bash --version 2>&1 | head -1)"
printf "%10s %s\n" "bash:" "${BASH_VERSION:-}"
printf "%10s %s\n" "cl:" "$(cl.exe --version 2>&1 | head -1)"
echo >&2
