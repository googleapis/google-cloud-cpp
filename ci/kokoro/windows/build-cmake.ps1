# !/usr/bin/env powershell
#
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
ForEach($var in ("CONFIG", "GENERATOR", "VCPKG_TRIPLET")) {
    if (-not (Test-Path env:${var})) {
        $missing+=(${var})
    }
}
if ($missing.count -ge 1) {
    Write-Host -ForegroundColor Red `
        "Aborting build because the ${missing} environment variables are not set."
    Exit 1
}

$binary_dir="cmake-out\msvc-${env:VCPKG_TRIPLET}"
# By default assume "module", use the configuration parameters and build in the `cmake-out` directory.
$cmake_flags=@("-G$env:GENERATOR", "-DCMAKE_BUILD_TYPE=$env:CONFIG", "-H.", "-B${binary_dir}")

# Setup the environment for vcpkg:
$project_root = (Get-Item -Path ".\" -Verbose).FullName
$cmake_flags += "-DCMAKE_TOOLCHAIN_FILE=`"${project_root}\cmake-out\vcpkg\scripts\buildsystems\vcpkg.cmake`""
$cmake_flags += "-DVCPKG_TARGET_TRIPLET=$env:VCPKG_TRIPLET"
$cmake_flags += "-DCMAKE_C_COMPILER=cl.exe"
$cmake_flags += "-DCMAKE_CXX_COMPILER=cl.exe"

# Configure CMake and create the build directory.
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Configuring CMake with $cmake_flags"
cmake $cmake_flags
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "cmake config failed with exit code $LastExitCode"
    Get-Content -Path "${binary_dir}/vcpkg-manifest-install.log"
    Exit ${LastExitCode}
}

# Workaround some flaky / broken tests in the CI builds, some of the
# tests where (reported) successfully created during the build,
# only to be missing when running the tests. This is probably a toolchain
# bug, and this seems to workaround it.
$workaround_targets=(
    # Failed around 2020-07-29
    "storage_internal_tuple_filter_test",
    # Failed around 2020-08-10
    "storage_well_known_parameters_test",
    # Failed around 2021-01-25
    "common_internal_random_test",
    "common_future_generic_test",
    "googleapis_download"
)
ForEach($target IN $workaround_targets) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling $target with CMake $env:CONFIG"
    cmake --build "${binary_dir}" --config $env:CONFIG --target "${target}"
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling with CMake $env:CONFIG"
cmake --build "${binary_dir}" --config $env:CONFIG
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "cmake for 'all' target failed with exit code $LastExitCode"
    Exit ${LastExitCode}
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Running unit tests $env:CONFIG"
Set-Location "${binary_dir}"

if (Test-Path env:RUNNING_CI) {
    # On Kokoro we need to define %TEMP% or the tests do not have a valid directory for
    # temporary files.
    $env:TEMP="T:\tmp"
}

# Get the number of processors to parallelize the tests
$NCPU=(Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors

$ctest_flags = @("--output-on-failure", "-j", $NCPU, "-C", $env:CONFIG)
ctest $ctest_flags -LE integration-test
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "ctest failed with exit code $LastExitCode"
    Exit ${LastExitCode}
}

if ((Test-Path env:RUN_INTEGRATION_TESTS) -and ($env:RUN_INTEGRATION_TESTS -eq "true")) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Running integration tests $env:CONFIG"
    ctest $ctest_flags -L integration-test
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "Integration tests failed with exit code $LastExitCode"
        Exit ${LastExitCode}
    }
}

function Integration-Tests-Enabled {
    if ((Test-Path env:KOKORO_GFILE_DIR) -and
        (Test-Path "${env:KOKORO_GFILE_DIR}/kokoro-run-key.json")) {
        return $True
    }
    return $False
}

if (Integration-Tests-Enabled) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Running integration tests $env:CONFIG"
    $integration_tests_config="${project_root}/ci/etc/integration-tests-config.ps1"
    . "${integration_tests_config}"
    ForEach($_ in (1, 2, 3)) {
        if ( $_ -ne 1) {
            Start-Sleep -Seconds (60 * $_)
        }
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Downloading roots.pem [$_]"
        try {
            (New-Object System.Net.WebClient).Downloadfile(
                    'https://pki.google.com/roots.pem',
                    "${env:KOKORO_GFILE_DIR}/roots.pem")
            break
        } catch {
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) download error"
        }
    }
    ${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}="${env:KOKORO_GFILE_DIR}/roots.pem"
    ${env:GOOGLE_APPLICATION_CREDENTIALS}="${env:KOKORO_GFILE_DIR}/kokoro-run-key.json"
    ${env:GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES}="yes"
    ${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON}="${env:KOKORO_GFILE_DIR}/kokoro-run-key.json"
    ${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_ROOTS_PEM}="${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}"
    ${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE}="${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json"
    ${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME}="${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json"

    Set-Location "${project_root}"
    Set-Location "${binary_dir}"
    # TODO(6062) - restore the GCS+gRPC tests (i.e. remove storage_grpc_ from the -E option)
    ctest $ctest_flags `
        -L 'integration-test-production' `
        -E '(bigtable_grpc_credentials|grpc_credential_types|storage_service_account_samples|service_account_integration_test|storage_grpc_)'
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "Integration tests failed with exit code ${LastExitCode}."
        Exit ${LastExitCode}
    }
    Set-Location "${project_root}"
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) DONE"
