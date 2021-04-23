#!/bin/bash
#
# Copyright 2021 Google LLC
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

# This library can be sourced by build scripts that need to run git commands.
# GCB does not provide our `.git` directory, so we need to create a temporary
# one here in order for git commands to work.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_GIT_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_GIT_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_GIT_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

io::log "Checking for a valid git environment..."
if [[ -d .git ]]; then
  io::log_green "Found .git directory"
else
  # This .git dir is thrown away at the end of the build.
  io::log "Initializing .git directory"
  git init --initial-branch=ephemeral-branch
  git config --local user.email "ephemeral@fake"
  git config --local user.name "ephemeral"
  git add .
  git commit --quiet -m "ephemeral: added all files"
fi
