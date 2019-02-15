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

if [ -z "${COLOR_RESET+x}" ]; then
  if type tput >/dev/null 2>&1; then
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
