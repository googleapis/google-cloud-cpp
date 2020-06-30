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

# First check the required environment variables.
$missing=@()
ForEach($var in ("BUILD_NAME")) {
    if (-not (Test-Path env:${var})) {
        $missing+=(${var})
    }
}
if ($missing.count -ge 1) {
    Write-Host -ForegroundColor Red `
        "Aborting build because the ${missing} environment variables are not set."
    Exit 1
}

# Create output directory for Bazel. Bazel creates really long paths,
# sometimes exceeding the Windows limits. Using a short name for the
# root of the Bazel output directory works around this problem.
$bazel_root="C:\b"
if (-not (Test-Path $bazel_root)) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Create bazel user root (${bazel_root})"
    New-Item -ItemType Directory -Path $bazel_root | Out-Null
}

$common_flags = @("--output_user_root=${bazel_root}")

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Capture Bazel information for troubleshooting"
bazel $common_flags version
bazel $common_flags shutdown

$GOOGLE_CLOUD_CPP_REPOSITORY="google-cloud-cpp"
$CACHE_BUCKET=if(Test-Path env:GOOGLE_CLOUD_CPP_KOKORO_RESULTS) `
    {$env:GOOGLE_CLOUD_CPP_KOKORO_RESULTS} else {"cloud-cpp-kokoro-results"}
$BRANCH_NAME="master"
$CACHE_FOLDER="${CACHE_BUCKET}/build-cache/${GOOGLE_CLOUD_CPP_REPOSITORY}/${BRANCH_NAME}"
# Bazel creates many links (aka NTFS JUNCTIONS) and only .tgz files seem to
# support those well.
$CACHE_BASENAME="cache-windows-${env:BUILD_NAME}"

$RunningCI = (Test-Path env:RUNNING_CI) -and `
    ($env:RUNNING_CI -eq "yes")
$IsCI = (Test-Path env:KOKORO_JOB_TYPE) -and `
    ($env:KOKORO_JOB_TYPE -eq "CONTINUOUS_INTEGRATION")
$IsPR = (Test-Path env:KOKORO_JOB_TYPE) -and `
    ($env:KOKORO_JOB_TYPE -eq "PRESUBMIT_GITHUB")
$CacheConfigured = (Test-Path env:KOKORO_GFILE_DIR) -and `
    (Test-Path "${env:KOKORO_GFILE_DIR}/build-results-service-account.json")
$Has7z = Get-Command "7z" -ErrorAction SilentlyContinue

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Shutting down Bazel server"
bazel $common_flags shutdown

$download_dir = "T:\tmp"
if (Test-Path env:TEMP) {
    $download_dir="${env:TEMP}"
} elseif (-not $download_dir) {
    Make-Item -Type "Directory" ${download_dir}
}
if ($RunningCI -and $IsPR -and $CacheConfigured -and $Has7z) {
    gcloud auth activate-service-account --key-file `
        "${env:KOKORO_GFILE_DIR}/build-results-service-account.json"
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
        "downloading Bazel cache."
    gsutil -q cp "gs://${CACHE_FOLDER}/${CACHE_BASENAME}.7z" "${download_dir}"
    if ($LastExitCode) {
        # Ignore errors, caching failures should not break the build.
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "gsutil download failed with exit code ${LastExitCode}."
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "continue building without a cache"
    } else {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "decompressing build cache."
        7z x "${download_dir}/${CACHE_BASENAME}.7z" "-o${download_dir}" "-aoa" "-bso0" "-bsp0"
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "extracting build cache."
        $extract_flags=@(
            # Preserve full path
            "-spf",
            # Preserve symbolic links
            "-snl",
            # Preserve hard links
            "-snh",
            # Overwrite all items
            "-aoa",
            # Suppress progress messages
            "-bsp0",
            # Suppress typicaly output messages
            "-bso0"
        )
        7z x "${download_dir}/${CACHE_BASENAME}.tar" ${extract_flags}
        if ($LastExitCode) {
            # Ignore errors, caching failures should not break the build.
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
                "extracting build cache failed with exit code ${LastExitCode}."
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
                "Continue building without a cache"
        }
    }
}

$test_flags = @("--test_output=errors",
                "--verbose_failures=true",
                "--keep_going")
$build_flags = @("--keep_going")

if (${env:BUILD_NAME} -eq "bazel-release") {
    $test_flags+=("-c", "opt")
    $build_flags+=("-c", "opt")
}

$env:BAZEL_VC="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC"

ForEach($_ in (1, 2, 3)) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Fetch dependencies [$_]"
    bazel $common_flags fetch -- //google/cloud/...:all
    if ($LastExitCode -eq 0) {
        break
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling and running unit tests"
bazel $common_flags test $test_flags `
  --test_tag_filters=-integration-test `
  -- //google/cloud/...:all
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "bazel test failed with exit code ${LastExitCode}."
    Exit ${LastExitCode}
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling extra programs"
bazel $common_flags build $build_flags -- //google/cloud/...:all
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "bazel test failed with exit code ${LastExitCode}."
    Exit ${LastExitCode}
}

function Integration-Tests-Enabled {
    if ((Test-Path env:KOKORO_GFILE_DIR) -and
        (Test-Path "${env:KOKORO_GFILE_DIR}/kokoro-run-key.json") -and
        (Test-Path "${env:KOKORO_GFILE_DIR}/kokoro-run-key.p12")) {
        return $True
    }
    return $False
}

$PROJECT_ROOT = (Get-Item -Path ".\" -Verbose).FullName
if (Integration-Tests-Enabled) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Running integration tests $env:CONFIG"
    $integration_tests_config="${PROJECT_ROOT}/ci/etc/integration-tests-config.ps1"
    . "${integration_tests_config}"
    (New-Object System.Net.WebClient).Downloadfile(
        'https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem',
         "${env:KOKORO_GFILE_DIR}/roots.pem")
    ${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}="${env:KOKORO_GFILE_DIR}/roots.pem"
    $enable_bigtable_admin_integration_tests="no"
    if (Test-Path env:ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS) {
        $enable_bigtable_admin_integration_tests=${env:ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS}
    }

    $integration_flags=@(
        # Common configuration
        "--test_env=GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}",
        "--test_env=GOOGLE_APPLICATION_CREDENTIALS=${env:KOKORO_GFILE_DIR}/kokoro-run-key.json",
        "--test_env=GOOGLE_CLOUD_PROJECT=${env:GOOGLE_CLOUD_PROJECT}",
        "--test_env=GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES=yes",

        # Bigtable
        "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID=${env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID}",
        "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A=${env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A}",
        "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B=${env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B}",
        "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT=${env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT}",
        "--test_env=ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS=${enable_bigtable_admin_integration_tests}",

        # Storage
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME=${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}",
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME=${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME}",
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID=${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID}",
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME=${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME}",
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT=${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT}",
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT=${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT}",
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY=${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY}",
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE=${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json",
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME=${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json",
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON=${env:KOKORO_GFILE_DIR}/kokoro-run-key.json",
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12=${env:KOKORO_GFILE_DIR}/kokoro-run-key.p12"
        # Spanner
        "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID=${env:GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID}"
        "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT=${env:GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT}"
    )
    bazel $common_flags test $test_flags $integration_flags `
        "--test_tag_filters=integration-test" `
        -- //google/cloud/...:all `
        -//google/cloud/bigtable/examples:bigtable_grpc_credentials `
        -//google/cloud/storage/examples:storage_service_account_samples `
        -//google/cloud/storage/tests:service_account_integration_test
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "Integration tests failed with exit code ${LastExitCode}."
        Exit ${LastExitCode}
    }
}

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Shutting down Bazel server"
bazel $common_flags shutdown
bazel shutdown

if ($RunningCI -and $IsCI -and $CacheConfigured -and $Has7z) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Updating Bazel cache"
    # We use 7z because it knows how to handle locked files better than Unix
    # tools like tar(1).
    $archive_flags=@(
        # Preserve hard links
        "-snh",
        # Preserve soft links
        "-snl",
        # Preserve full path
        "-spf",
        # Exclude directories named "install"
        "-xr!install",
        # Suppress standard logging
        "-bso0",
        # Suppress progress
        "-bsp0"
    )
    Remove-Item "${download_dir}\${CACHE_BASENAME}.tar" -ErrorAction SilentlyContinue
    7z a "${download_dir}\${CACHE_BASENAME}.tar" "${bazel_root}" ${archive_flags}
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compressing cache tarball"
    Remove-Item "${download_dir}\${CACHE_BASENAME}.7z" -ErrorAction SilentlyContinue
    7z a "${download_dir}\${CACHE_BASENAME}.7z" "${download_dir}\${CACHE_BASENAME}.tar" -bso0 -bsp0
    if ($LastExitCode) {
        # Just report these errors and continue, caching failures should
        # not break the build.
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "zipping cache contents failed with exit code ${LastExitCode}."
    } else {
        gcloud auth activate-service-account --key-file `
            "${env:KOKORO_GFILE_DIR}/build-results-service-account.json"
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "uploading Bazel cache."
        gsutil -q cp "${download_dir}\${CACHE_BASENAME}.7z" "gs://${CACHE_FOLDER}/${CACHE_BASENAME}.7z"
        if ($LastExitCode) {
            # Just report these errors and continue, caching failures should
            # not break the build.
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
                "uploading cache failed exit code ${LastExitCode}."
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
                "cache not updated, this will not cause a build failure."
        }
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) DONE"
