#!/bin/bash
#
# Copyright 2021 Google LLC
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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/integration.sh

export CC=gcc
export CXX=g++

# Explicitly list the patterns that match hand-crafted code. Excluding the
# generated code results in a longer list and more maintenance.
instrumented_patterns=(
  "/examples[/:]"
  "/generator[/:]"
  "/google/cloud:"
  "/google/cloud/testing_util:"
  "/google/cloud/bigtable[/:]"
  "/google/cloud/pubsub[/:]"
  "/google/cloud/pubsublite[/:]"
  "/google/cloud/spanner[/:]"
  "/google/cloud/storage[/:]"
)
instrumentation_filter="$(printf ",%s" "${instrumented_patterns[@]}")"
instrumentation_filter="${instrumentation_filter:1}"

mapfile -t args < <(bazel::common_args)
args+=("--instrumentation_filter=${instrumentation_filter}")
args+=("--instrument_test_targets")
# This is a workaround for:
#     https://github.com/googleapis/google-cloud-cpp/issues/6952
# Based on the recommendations from:
#     https://github.com/bazelbuild/bazel/issues/3236
args+=("--sandbox_tmpfs_path=/tmp")
bazel coverage "${args[@]}" --test_tag_filters=-integration-test ...
GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS="instance"
mapfile -t integration_args < <(integration::bazel_args)
integration::bazel_with_emulators coverage "${args[@]}" "${integration_args[@]}"

io::log_h2 "Running Storage integration tests (with emulator and Legacy http)"
"google/cloud/storage/ci/run_integration_tests_emulator_bazel.sh" \
  bazel coverage "${args[@]}" "${integration_args[@]}" \
  "--test_env=GOOGLE_CLOUD_CPP_STORAGE_USE_LEGACY_HTTP=yes"

# Where does this token come from? For triggered ci/pr builds GCB will securely
# inject this into the environment. See the "secretEnv" setting in the
# cloudbuild.yaml file. The value is stored in Secret Manager. You can store
# your own token in your personal project's Secret Manager so that your
# personal builds have coverage data uploaded to your own account. See also
# https://cloud.google.com/build/docs/securing-builds/use-secrets
if [[ -z "${CODECOV_TOKEN:-}" ]]; then
  io::log_h2 "No codecov token. Skipping upload."
  exit 0
fi

# Merges the coverage.dat files, which reduces the overall size by about 90%.
readonly MERGED_COVERAGE="/var/tmp/merged-coverage.lcov"
io::log_h2 "Merging coverage data into ${MERGED_COVERAGE}"
TIMEFORMAT="==> 🕑 merging done in %R seconds"
time {
  mapfile -t coverage_dat < <(find "$(bazel info output_path)" -name "coverage.dat")
  io::log "Found ${#coverage_dat[@]} coverage.dat files"
  mapfile -t lcov_flags < <(printf -- "--add-tracefile=%s\n" "${coverage_dat[@]}")
  lcov --quiet "${lcov_flags[@]}" --output-file "${MERGED_COVERAGE}"
  ls -lh "${MERGED_COVERAGE}"
}

codecov_args=(
  "--filter=gcov"
  "--file=${MERGED_COVERAGE}"
  "--branch=${BRANCH_NAME}"
  "--sha=${COMMIT_SHA}"
  "--pr=${PR_NUMBER:-}"
  "--build=${BUILD_ID:-}"
  "--verbose"
)
io::log_h2 "Uploading ${MERGED_COVERAGE} to codecov.io"
io::log "Flags: ${codecov_args[*]}"
TIMEFORMAT="==> 🕑 codecov.io upload done in %R seconds"
time {
  # Downloads and verifies the codecov uploader before executing it.
  codecov="$(mktemp -u -t codecov.XXXXXXXXXX)"
  curl -sSL -o "${codecov}" https://uploader.codecov.io/v0.3.1/linux/codecov
  sha256sum="0146032e40bc0179db3afa3de179dab4da59ece0449af03d881f3c509eaabc8b"
  if ! sha256sum -c <(echo "${sha256sum} *${codecov}"); then
    io::log_h2 "ERROR: Invalid sha256sum for codecov program"
    exit 1
  fi
  chmod +x "${codecov}"
  env -i HOME="${HOME}" "${codecov}" --token="${CODECOV_TOKEN}" "${codecov_args[@]}"
  rm "${codecov}"
}
