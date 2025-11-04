#!/usr/bin/env powershell
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
# See #15678
$exclude_build_targets = @("-//google/cloud/bigtable:internal_query_plan_test", `
    "-//google/cloud/storage/tests:storage_include_test-default", `
    "-//google/cloud/storage/tests:storage_include_test-grpc-metadata", `
    "-//google/cloud/pubsub/samples:all")
bazelisk $common_flags test $test_flags --test_tag_filters=-integration-test ... -- $exclude_build_targets
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "bazel test failed with exit code ${LastExitCode}."
    Exit ${LastExitCode}
}

Write-Host "`n$(Get-Date -Format o) Compiling extra programs with bazel $common_flags build $build_flags ..."
bazelisk $common_flags build $build_flags ... -- $exclude_build_targets
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "bazel build failed with exit code ${LastExitCode}."
    Exit ${LastExitCode}
}

# Import the functions and variables used to run integration tests
. ci/kokoro/windows/lib/integration.ps1

function Invoke-REST-Quickstart {
    param($bazel_bin)
    try {
        $executable = Join-Path $bazel_bin "google/cloud/storage/quickstart/quickstart.exe"
        Write-Host "Running REST Quickstart, attempting to run: $executable"
        if (-not (Test-Path $executable)) {
          Write-Host -ForegroundColor Red "Executable not found at the specified path."
          Exit 1
        }
        & $executable "${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
        if ($LastExitCode) {
            Write-Host -ForegroundColor Red "Execution of (storage/quickstart) failed with exit code ${LastExitCode}."
            Exit ${LastExitCode}
        }
    } catch {
        Write-Host -ForegroundColor Red "Caught exception while trying to run storage/quickstart: $_"
        Exit 1
    }
}

function Invoke-gRPC-Quickstart {
    param($bazel_bin)
    try {
        $executable = Join-Path $bazel_bin "google/cloud/pubsub/quickstart/quickstart.exe"
        Write-Host "Running gRPC Quickstart, attempting to run: $executable"
        if (-not (Test-Path $executable)) {
          Write-Host -ForegroundColor Red "Executable not found at the specified path."
          Exit 1
        }
        & $executable "${env:GOOGLE_CLOUD_PROJECT}" "${env:GOOGLE_CLOUD_CPP_PUBSUB_TEST_QUICKSTART_TOPIC}"
        if ($LastExitCode) {
            Write-Host -ForegroundColor Red "Execution of (pubsub/quickstart) failed with exit code ${LastExitCode}."
            Exit ${LastExitCode}
        }
    } catch {
        Write-Host -ForegroundColor Red "Caught exception while trying to run pubsub/quickstart: $_"
        Exit 1
    }
}

if (Test-Integration-Enabled) {
    Write-Host "`n$(Get-Date -Format o) Running minimal quickstart prorams"
    Install-Roots-Pem
    $env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH = Join-Path $env:KOKORO_GFILE_DIR "roots.pem"
    $env:CURL_CA_BUNDLE = Join-Path $env:KOKORO_GFILE_DIR "roots.pem"
    $env:GOOGLE_APPLICATION_CREDENTIALS = Join-Path $env:KOKORO_GFILE_DIR "kokoro-run-key.json"
    # Troubleshooting output
    Write-Host "GOOGLE_APPLICATION_CREDENTIALS=$env:GOOGLE_APPLICATION_CREDENTIALS"
    Write-Host "GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=$env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH"
    Write-Host "CURL_CA_BUNDLE=$env:CURL_CA_BUNDLE"
    Get-Content (Join-Path $env:KOKORO_GFILE_DIR "roots.pem") -TotalCount 5

    bazelisk $common_flags build $build_flags `
        //google/cloud/storage/quickstart:quickstart `
        //google/cloud/pubsub/quickstart:quickstart

    $bazel_bin = (bazelisk $common_flags info $build_flags bazel-bin).Trim()
    $bazel_bin = $bazel_bin.Replace('/', '\')
    Write-Host "bazel-bin directory: $bazel_bin"

    # DEBUG START: List all quickstart.exe files found in the output directory
    Write-Host "DEBUG: Searching for 'quickstart.exe' in $bazel_bin..."
    try {
        Get-ChildItem -Path $bazel_bin -Filter "quickstart.exe" -Recurse -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "Found: $($_.FullName)" }
    } catch {
        Write-Host "DEBUG: Failed to list contents of $bazel_bin"
    }
    # DEBUG END

    # --- NEW DEBUGGING FLAGS ---
    # Enable detailed logging in the Google Cloud C++ Client
    # "http" logs the headers/payloads (sanitized) and handshake info.
    $env:GOOGLE_CLOUD_CPP_ENABLE_TRACING="http"
    $env:CURL_VERBOSE=1

    Invoke-REST-Quickstart $bazel_bin
    Invoke-gRPC-Quickstart $bazel_bin
}

# Shutdown the Bazel server to release any locks
Write-Host "$(Get-Date -Format o) Shutting down Bazel server"
bazelisk $common_flags shutdown
bazelisk shutdown

Write-Host "`n$(Get-Date -Format o) DONE"
