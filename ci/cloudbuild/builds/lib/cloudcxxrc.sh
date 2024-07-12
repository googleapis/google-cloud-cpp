#!/bin/bash
#
# Copyright 2024 Google LLC
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
test -n "${CI_CLOUDBUILD_BUILDS_LIB_CLOUDCXXRC_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_CLOUDCXXRC_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_CLOUDCXXRC_SH__++ != 0)); then
  return 0
fi # include guard

# Load build customizations. Start with the default, then any settings for
# all workspaces, then the settings for the current workspace.
#
# Developers only need to override what changes, keep a configuration file that
# applies to all their builds, and have some workspaces with custom settings.
source module ci/etc/cloudcxxrc
if [[ -r "${HOME}/.cloudcxxrc" ]]; then
  source "${HOME}/.cloudcxxrc"
fi
if [[ -r "${PROJECT_ROOT}/.cloudcxxrc" ]]; then
  source "${PROJECT_ROOT}/.cloudcxxrc"
fi

function bazel::has_no_tests() {
  local target=$1
  shift
  if [[ "${#BAZEL_TARGETS[@]}" -eq 0 ]]; then
    return 0
  fi
  query_expr="$(printf '+ %s' "${BAZEL_TARGETS[@]}")"
  query_expr="tests(${query_expr:2} intersect ${target})"
  # Tolerate failures in `bazel` as a workaround for:
  #   https://github.com/bazelbuild/bazel/issues/14787
  if ! (bazel query --noshow_progress "${query_expr}" || true) | grep -q "${target}"; then
    return 0
  fi
  return 1
}
