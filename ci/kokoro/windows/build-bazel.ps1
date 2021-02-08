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

$BuildName = ""
if ($args.count -eq 1) {
    $BuildName = $args[0]
} else {
    Write-Host -ForegroundColor Red `
        "Aborting build, expected the build name as the first (and only) argument"
    Exit 1
}

# Create output directory for Bazel. Bazel creates really long paths,
# sometimes exceeding the Windows limits. Using a short name for the
# root of the Bazel output directory works around this problem.
# We do this in C: (the boot disk) because T: (the tmpfs / build disk)
# is too small for the Bazel directory.
$bazel_root="C:\b"
if (-not (Test-Path $bazel_root)) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Create bazel user root (${bazel_root})"
    New-Item -ItemType Directory -Path $bazel_root | Out-Null
}

$common_flags = @("--output_user_root=${bazel_root}")

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Capture Bazel information for troubleshooting"
bazel $common_flags version

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Shutting down Bazel server"
bazel $common_flags shutdown

$download_dir = "T:\tmp"
if (Test-Path env:TEMP) {
    $download_dir="${env:TEMP}"
} elseif (-not (Test-Path "${download_dir}")) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Create download directory ${download_dir}"
    Make-Item -Type "Directory" ${download_dir}
}
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Using ${download_dir} as download directory"

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Disk(s) size and space for troubleshooting"
Get-CimInstance -Class CIM_LogicalDisk | `
    Select-Object -Property DeviceID, DriveType, VolumeName, `
        @{L='FreeSpaceGB';E={"{0:N2}" -f ($_.FreeSpace /1GB)}}, `
        @{L="Capacity";E={"{0:N2}" -f ($_.Size/1GB)}}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Bazel Root (${bazel_root}) size"
Get-Item -ErrorAction SilentlyContinue "${bazel_root}"  | `
    Get-ChildItem -ErrorAction SilentlyContinue -Recurse | `
    Measure-Object -ErrorAction SilentlyContinue -Sum Length | `
    Select-Object Count, @{L="SizeGB";E={"{0:N2}" -f ($_.Sum / 1GB)}}

$build_flags = @(
    "--keep_going",
    "--per_file_copt=^//google/cloud@-W3",
    "--per_file_copt=^//google/cloud@-WX",
    "--per_file_copt=^//google/cloud@-experimental:external",
    "--per_file_copt=^//google/cloud@-external:W0",
    "--per_file_copt=^//google/cloud@-external:anglebrackets",
    # Disable warnings on generated proto files.
    "--per_file_copt=.*\.pb\.cc@/wd4244"
)

$BAZEL_CACHE="https://storage.googleapis.com/cloud-cpp-bazel-cache"

# If we have the right credentials, tell bazel to cache build results in a GCS
# bucket. Note: this will not cache external deps, so the "fetch" below will
# not hit this cache.
if ((Test-Path env:KOKORO_GFILE_DIR) -and
    (Test-Path "${env:KOKORO_GFILE_DIR}/kokoro-run-key.json")) {
    Write-Host -ForegroundColor Yellow "Using bazel remote cache: ${BAZEL_CACHE}/windows/${BuildName}"
    $build_flags += @("--remote_cache=${BAZEL_CACHE}/windows/${BuildName}")
    $build_flags += @("--google_credentials=${env:KOKORO_GFILE_DIR}/kokoro-run-key.json")
    # See https://docs.bazel.build/versions/master/remote-caching.html#known-issues
    # and https://github.com/bazelbuild/bazel/issues/3360
    $build_flags += @("--experimental_guard_against_concurrent_changes")
}

if ($BuildName -eq "bazel-release") {
    $build_flags += ("-c", "opt")
}

$env:BAZEL_VC="C:\Program Files (x86)\Microsoft Visual Studio\${env:MSVC_VERSION}\Community\VC"

ForEach($_ in (1, 2, 3)) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Fetch dependencies [$_]"
    # Additional dependencies, these are not downloaded by `bazel fetch ...`,
    # but are needed to compile the code
    $external=@(
        "@local_config_platform//...",
        "@local_config_cc_toolchains//...",
        "@local_config_sh//...",
        "@go_sdk//...",
        "@remotejdk11_win//:jdk"
    )
    if ($LastExitCode -eq 0) {
        break
    }
    Start-Sleep -Seconds (60 * $_)
}

# All the build_flags should be set by now, so we'll copy them, and add a few
# more test-only flags.
$test_flags = $build_flags
$test_flags += @("--test_output=errors", "--verbose_failures=true")

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling and running unit tests"
bazel $common_flags test $test_flags --test_tag_filters=-integration-test ...
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "bazel test failed with exit code ${LastExitCode}."
    Exit ${LastExitCode}
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling extra programs"
Write-Host -ForegroundColor Yellow bazel $common_flags build $build_flags ...
bazel $common_flags build $build_flags ...
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
    ForEach($_ in (1, 2, 3)) {
        if ( $_ -ne 1) {
            Start-Sleep -Seconds (60 * $_)
        }
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Downloading roots.pem [$_]"
        try {
            (New-Object System.Net.WebClient).Downloadfile(
                    'https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem',
                    "${env:KOKORO_GFILE_DIR}/roots.pem")
            break
        } catch {
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) download error"
        }
    }
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
        "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12=${env:KOKORO_GFILE_DIR}/kokoro-run-key.p12",
        # Spanner
        "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID=${env:GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID}",
        "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT=${env:GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT}",

        # IAM
        "--test_env=GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT=${env:GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT}",
        "--test_env=GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT=${env:GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT}"
    )
    bazel $common_flags test $test_flags $integration_flags `
        "--test_tag_filters=integration-test" `
        -- ... `
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

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Disk(s) size and space for troubleshooting"
Get-CimInstance -Class CIM_LogicalDisk | `
    Select-Object -Property DeviceID, DriveType, VolumeName, `
        @{L='FreeSpaceGB';E={"{0:N2}" -f ($_.FreeSpace /1GB)}}, `
        @{L="Capacity";E={"{0:N2}" -f ($_.Size/1GB)}}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Bazel Root (${bazel_root}) size"
Get-Item -ErrorAction SilentlyContinue "${bazel_root}"  | `
    Get-ChildItem -ErrorAction SilentlyContinue -Recurse | `
    Measure-Object -ErrorAction SilentlyContinue -Sum Length | `
    Select-Object Count, @{L="SizeGB";E={"{0:N2}" -f ($_.Sum / 1GB)}}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) DONE"
