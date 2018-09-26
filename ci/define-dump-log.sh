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

################################################
# Dump a log file to the console without going over CI system limits.
# Globals:
#   None
# Arguments:
#   logfile: the name of the logfile to print out.
# Returns:
#   None
################################################
dump_log() {
  local -r logfile=$1
  shift
  local -r base="$(basename "${logfile}")"

  if [ ! -r "${logfile}" ]; then
    return
  fi

  echo "================ [begin ${base}] ================"
  # Travis has a limit of ~10,000 lines, if the file is around 1,000 just print
  # the full file.
  if [ "$(wc -l "${logfile}" | awk '{print $1}')" -lt 200 ]; then
    cat "${logfile}"
  else
    head -100 "${logfile}"
    echo "==== [snip] [snip] [${base}] [snip] [snip] ===="
    tail -100 "${logfile}"
  fi
  echo "================ [end ${base}] ================"
}
