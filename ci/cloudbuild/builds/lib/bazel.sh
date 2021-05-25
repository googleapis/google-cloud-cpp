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

# This bash library has various helper functions for our bazel-based builds
# and automatically pre-fetches all dependencies for the project.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_BAZEL_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_BAZEL_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_BAZEL_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

io::log "Prefetching bazel deps..."
# Bazel downloads all the dependencies of a project, as well as a number of
# development tools during startup. In automated builds these downloads fail
# from time to time due to transient network problems. Running `bazel fetch` at
# the beginning of the build prevents such transient failures from flaking the
# build.
TIMEFORMAT="==> 🕑 prefetching done in %R seconds"
time {
  "ci/retry-command.sh" 3 120 bazel fetch ... \
    @local_config_platform//... \
    @local_config_cc_toolchains//... \
    @local_config_sh//... \
    @go_sdk//... \
    @remotejdk11_linux//:jdk
}
echo >&2

# Outputs a list of args that should be given to all bazel invocations. To read
# this into an array use `mapfile -t my_array < <(bazel::common_args)`
function bazel::common_args() {
  local args=(
    "--test_output=errors"
    "--verbose_failures=true"
    "--keep_going"
    "--experimental_convenience_symlinks=ignore"
  )
  if [[ -n "${BAZEL_REMOTE_CACHE:-}" ]]; then
    args+=("--remote_cache=${BAZEL_REMOTE_CACHE}")
    args+=("--google_default_credentials")
    # See https://docs.bazel.build/versions/master/remote-caching.html#known-issues
    # and https://github.com/bazelbuild/bazel/issues/3360
    args+=("--experimental_guard_against_concurrent_changes")
  fi
  printf "%s\n" "${args[@]}"
}
