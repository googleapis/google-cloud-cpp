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
test -n "${CI_CLOUDBUILD_BUILDS_LIB_BAZEL_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_BAZEL_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_BAZEL_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

io::log "Using bazelisk version"
bazelisk version

# Outputs a list of args that should be given to all bazel invocations. To read
# this into an array use `mapfile -t my_array < <(bazel::common_args)`
function bazel::common_args() {
  if [[ -z "${BAZEL_ROOT:-}" ]]; then
    return 0
  fi
  local args=("--output_user_root=${BAZEL_ROOT}")
  printf "%s\n" "${args[@]}"
}

# Outputs a list of args that should be given to all `bazel test` invocations.
function bazel::test_args() {
  local args=(
    "--test_output=errors"
    "--verbose_failures=true"
    "--keep_going"
    "--experimental_convenience_symlinks=ignore"
  )
  if [[ -n "${BAZEL_REMOTE_CACHE:-}" ]]; then
    args+=("--remote_cache=${BAZEL_REMOTE_CACHE}")
    args+=("--google_default_credentials")
    # See https://docs.bazel.build/versions/main/remote-caching.html#known-issues
    # and https://github.com/bazelbuild/bazel/issues/3360
    args+=("--experimental_guard_against_concurrent_changes")
  fi
  printf "%s\n" "${args[@]}"
}

# Outputs a list of args that should be given when using bazel and MSVC.
function bazel::msvc_args() {
  local args=(
    '--keep_going'
    # Disable warnings on "external" headers. These are non-actionable, or at
    # least not urgent, as they are in generated code or code we do not control.
    # This is the motivation to include things outside the project with angle
    # brackets. There is no other succinct way to tell MSVC what is "external".
    '--per_file_copt=^//google/cloud@-experimental:external'
    '--per_file_copt=^//google/cloud@-external:W0'
    '--per_file_copt=^//google/cloud@-external:anglebrackets'
    # Disable warnings in generated proto and upb files. This is not strictly
    # needed, but reduces some of the noise in the build logs. Note that the
    # headers for these files continue to emit warnings. They are included by
    # files in gRPC and Protobuf.
    '--per_file_copt=.*\.pb\.cc@-wd4244'
    '--per_file_copt=.*\.pb\.cc@-wd4267'
    '--per_file_copt=.*\.upb\.c@-wd4090'
    '--per_file_copt=.*\.upbdefs\.c@-wd4090'
  )
  printf "%s\n" "${args[@]}"
}

# Outputs a list of args to use with integration tests
function bazel::integration_test_args() {
  local args=(
    --flaky_test_attempts=3
    --test_tag_filters=integration-test-gha
    --test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME="${GHA_TEST_BUCKET:-}"
    --test_env=GOOGLE_APPLICATION_CREDENTIALS="${GOOGLE_APPLICATION_CREDENTIALS:-}"
    --test_env=HOME="${HOME:-}"
  )
  printf "%s\n" "${args[@]}"
}

# Bazel downloads all the dependencies of a project, as well as a number of
# development tools during startup. In automated builds these downloads fail
# from time to time due to transient network problems. Running `bazel fetch` at
# the beginning of the build prevents such transient failures from flaking the
# build.
function bazel::prefetch() {
  local args
  mapfile -t args < <(bazel::common_args)
  local common_rules=(
    "..."
  )
  local os_rules
  mapfile -t os_rules < <(os::prefetch)
  "ci/retry-command.sh" 3 120 bazelisk "${args[@]}" fetch "${common_rules[@]}"
}

io::log "Prefetching bazel deps..."
TIMEFORMAT="==> 🕑 prefetching done in %R seconds"
time {
  bazel::prefetch
}
echo >&2
