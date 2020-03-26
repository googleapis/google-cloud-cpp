# !/usr/bin/env powershell
# Copyright 2020 Google LLC
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

# Stop on errors. This is similar to `set -e` on Unix shells.
$ErrorActionPreference = "Stop"

$GOOGLE_CLOUD_CPP_REPOSITORY="google-cloud-cpp"
$CACHE_BUCKET=if(Test-Path env:GOOGLE_CLOUD_CPP_KOKORO_RESULTS) `
    {$env:GOOGLE_CLOUD_CPP_KOKORO_RESULTS} else {"cloud-cpp-kokoro-results"}
$BRANCH_NAME="master"
$CACHE_FOLDER="${CACHE_BUCKET}/build-cache/${GOOGLE_CLOUD_CPP_REPOSITORY}/${BRANCH_NAME}"
$CACHE_NAME="cache-windows-bazel.zip"

$IsPR = (Test-Path env:KOKORO_JOB_TYPE) -and `
    ($env:KOKORO_JOB_TYPE = "GITHUB_PULL_REQUEST")
$CacheConfigured = (Test-Path env:KOKORO_GFILE_DIR) -and `
    (Test-Path "${env:KOKORO_GFILE_DIR}/build-results-service-account.json")
$Has7z= Get-Command "7z" -ErrorAction SilentlyContinue

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Shutting down Bazel server"
bazel shutdown

if ($CacheConfigured -and $Has7z) {
    gcloud auth activate-service-account --key-file "${env:KOKORO_GFILE_DIR}/build-results-service-account.json"
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Downloading Bazel cache."
    gsutil -q cp "gs://${CACHE_FOLDER}/${CACHE_NAME}" "${CACHE_NAME}"
    if ($LastExitCode) {
        # Ignore errors, caching failures should not break the build.
        Write-Host "gsutil download failed with exit code $LastExitCode"
        Write-Host "Continue building without a cache"
    } else {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Extracting build cache."
        7z x "${CACHE_NAME}" -spf -aoa -bsp0
        if ($LastExitCode) {
            # Ignore errors, caching failures should not break the build.
            Write-Host "extracting build cache failed with exit code $LastExitCode"
            Write-Host "Continue building without a cache"
        }
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Capture Bazel information for troubleshooting"
bazel version

# Create output directory for Bazel. Bazel creates really long paths,
# sometimes exceeding the Windows limits. Using a short name for the
# root of the Bazel output directory works around this problem.
$bazel_root="C:\b"
if (-not (Test-Path $bazel_root)) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Create bazel user root (${bazel_root})"
    New-Item -ItemType Directory -Path $bazel_root | Out-Null
}

$common_flags = @("--output_user_root=${bazel_root}")

$test_flags = @("--test_output=errors",
                "--verbose_failures=true",
                "--keep_going")
$build_flags = @("--keep_going")

$env:BAZEL_VC="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC"

1..3 | ForEach-Object {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Fetch dependencies [$_]"
    bazel $common_flags fetch -- //google/cloud/...:all
    if ($LastExitCode -eq 0) {
        return
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling and running unit tests"
bazel $common_flags test $test_flags `
  --test_tag_filters=-integration-tests `
  -- //google/cloud/...:all
if ($LastExitCode) {
    throw "bazel test failed with exit code $LastExitCode"
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling extra programs"
bazel $common_flags build $build_flags -- //google/cloud/...:all
if ($LastExitCode) {
    throw "bazel test failed with exit code $LastExitCode"
}

if ((Test-Path env:RUN_INTEGRATION_TESTS) -and ($env:RUN_INTEGRATION_TESTS -eq "true")) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Running integration tests $env:CONFIG"
    $integration_flags=@(
        "--test_env", "GOOGLE_CLOUD_PROJECT=${env:GOOGLE_CLOUD_PROJECT}",
        "--test_env", "GOOGLE_APPLICATION_CREDENTIALS=${env:GOOGLE_APPLICATION_CREDENTIALS}",
        "--test_env", "GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES=yes",
        "--test_env", "RUN_SLOW_INTEGRATION_TESTS=${env:RUN_SLOW_INTEGRATION_TESTS}",
        "--test_env", "GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}"
    )
    bazel $common_flags test $test_flags $integration_flags `
      --test_tag_filters=integration-tests `
      -- //google/cloud/...:all
    if ($LastExitCode) {
        throw "Integration tests failed with exit code $LastExitCode"
    }
}

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Shutting down Bazel server"
bazel shutdown

if (# TODO(coryan) restore before merging: -not $IsPR -and
    $CacheConfigured -and $Has7z) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Updating Bazel cache"
    7z a "${CACHE_NAME}" "${bazel_root}" -spf -bso0 -bse0 -bsp0
    if ($LastExitCode) {
        # Ignore errors, caching failures should not break the build.
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) zipping cache "`
            "contents failed with exit code $LastExitCode"
    }
    if (Test-Path "${CACHE_NAME}") {
        gcloud auth activate-service-account --key-file "${env:KOKORO_GFILE_DIR}/build-results-service-account.json"
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Uploading Bazel cache."
        gsutil -q cp  "${CACHE_NAME}" "gs://${CACHE_FOLDER}/${CACHE_NAME}"
        if ($LastExitCode) {
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) uploading cache failed exit code $LastExitCode"
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) cache not updated"
        }
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) DONE"

Exit 0
