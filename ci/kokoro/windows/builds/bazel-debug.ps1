#!/usr/bin/env powershell
#
# Copyright 2022 Google LLC
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

# Stop on errors. This is similar to `set -e` on Unix shells.
$ErrorActionPreference = "Stop"

if ($args.count -ne 1) {
    Write-Host -ForegroundColor Red `
        "Aborting build, expected the build name as the first (and only) argument"
    Exit 1
}
$BuildName = $args[0]

. ci/kokoro/windows/lib/bazel.ps1

$common_flags = Get-Bazel-Common-Flags

Write-Bazel-Config

$build_flags = Get-Bazel-Build-Flags "${BuildName}"

Fetch-Bazel-Dependencies

# All the build_flags should be set by now, so we'll copy them, and add a few
# more test-only flags.
$test_flags = $build_flags
$test_flags += @("--test_output=errors", "--verbose_failures=true")

Write-Host "`n$(Get-Date -Format o) Compiling and running unit tests"
$targets = @(
    "//google/cloud:all",
    "//google/cloud/bigquery/...",
    "//google/cloud/bigtable/...",
    "//google/cloud/iam/...",
    "//google/cloud/pubsub/...",
    "//google/cloud/pubsublite/...",
    "//google/cloud/spanner/...",
    "//google/cloud/storage/..."
)
bazelisk $common_flags test $test_flags --test_tag_filters=-integration-test `
    ${targets}
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "bazel test failed with exit code ${LastExitCode}."
    Exit ${LastExitCode}
}

# Import the functions and variables used to run integration tests
. ci/kokoro/windows/lib/integration.ps1

function Invoke-REST-Quickstart {
    bazelisk $common_flags run $build_flags `
      //google/cloud/storage/quickstart:quickstart -- `
      "${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "bazel run (storage/quickstart) failed with exit code ${LastExitCode}."
        Exit ${LastExitCode}
    }
}

function Invoke-gRPC-Quickstart {
    bazelisk $common_flags run $build_flags `
      //google/cloud/pubsub/quickstart:quickstart -- `
      "${env:GOOGLE_CLOUD_PROJECT}" "${env:GOOGLE_CLOUD_CPP_PUBSUB_TEST_QUICKSTART_TOPIC}"
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "bazel run (pubsub/quickstart) failed with exit code ${LastExitCode}."
        Exit ${LastExitCode}
    }
}

if (Test-Integration-Enabled) {
    Write-Host "`n$(Get-Date -Format o) Running minimal quickstart prorams"
    Install-Roots-Pem
    ${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}="${env:KOKORO_GFILE_DIR}/roots.pem"
    ${env:GOOGLE_APPLICATION_CREDENTIALS}="${env:KOKORO_GFILE_DIR}/kokoro-run-key.json"
    Invoke-REST-Quickstart
    Invoke-gRPC-Quickstart
}

# Shutdown the Bazel server to release any locks
Write-Host "$(Get-Date -Format o) Shutting down Bazel server"
bazelisk $common_flags shutdown
bazelisk shutdown

Write-Host "`n$(Get-Date -Format o) DONE"
