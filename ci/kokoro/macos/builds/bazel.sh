#!/usr/bin/env bash
#
# Copyright 2020 Google LLC
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

set -euo pipefail

source "$(dirname "$0")/../../../lib/init.sh"
source module ci/etc/integration-tests-config.sh
source module ci/lib/io.sh

# NOTE: In this file use the command `bazelisk` rather than bazel, because
# Kokoro has both installed and we want to make sure to use the former.
io::log_h2 "Using bazel version"
: "${USE_BAZEL_VERSION:="7.6.1"}"
export USE_BAZEL_VERSION
bazelisk version || rm -fr "$HOME"/Library/Caches/bazelisk || bazelisk version

bazel_args=(
  # We need this environment variable because on macOS gRPC crashes if it
  # cannot find the credentials, even if you do not use them. Some of the
  # unit tests do exactly that.
  "--action_env=GOOGLE_APPLICATION_CREDENTIALS=${GOOGLE_APPLICATION_CREDENTIALS}"
  # This tells Bazel's toolchain detector which SDK to use, solving the
  # boringssl header conflict AND passing the hermeticity checks.
  "--action_env=SDKROOT=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
  # This manually enables the SSE4.2 and CRC32C instruction sets needed by
  # the crc32c library.
  "--copt:@crc32c//...=-msse4.2"
  "--copt:@crc32c//...=-mcrc32"
  "--test_output=errors"
  "--verbose_failures=true"
  "--keep_going"
  "--sandbox_debug"
)

readonly CONFIG_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly TEST_KEY_FILE_JSON="${CONFIG_DIR}/kokoro-run-key.json"
readonly BAZEL_CACHE="https://storage.googleapis.com/cloud-cpp-bazel-cache"

# If we have the right credentials, tell bazel to cache build results in a GCS
# bucket. Note: this will not cache external deps, so the "fetch" below will
# not hit this cache.
if [[ -r "${TEST_KEY_FILE_JSON}" ]]; then
  io::log "Using bazel remote cache: ${BAZEL_CACHE}/macos/${BUILD_NAME:-}"
  bazel_args+=(
    "--remote_cache=${BAZEL_CACHE}/macos/${BUILD_NAME:-}"
  )
  bazel_args+=("--google_credentials=${TEST_KEY_FILE_JSON}")
  # See https://docs.bazel.build/versions/main/remote-caching.html#known-issues
  # and https://github.com/bazelbuild/bazel/issues/3360
  bazel_args+=("--experimental_guard_against_concurrent_changes")
fi

io::log_h2 "build and run unit tests"
echo "bazel test " "${bazel_args[@]}"
bazelisk test "${bazel_args[@]}" "--test_tag_filters=-integration-test" ...

io::log_h2 "build all targets"
bazelisk build "${bazel_args[@]}" ...

io::log_h2 "running minimal quickstart programs"
bazelisk run "${bazel_args[@]}" \
  //google/cloud/storage/quickstart:quickstart -- \
  "${GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"

bazelisk run "${bazel_args[@]}" \
  //google/cloud/pubsub/quickstart:quickstart -- \
  "${GOOGLE_CLOUD_PROJECT}" "${GOOGLE_CLOUD_CPP_PUBSUB_TEST_QUICKSTART_TOPIC}"

# Kokoro needs bazel to be shutdown.
bazelisk shutdown
