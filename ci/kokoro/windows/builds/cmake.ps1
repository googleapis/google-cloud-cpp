# !/usr/bin/env powershell
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

# Load the functions to configure and use vcpkg.
. ci/kokoro/windows/lib/vcpkg.ps1

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

$packages = @("zlib", "openssl",
              "protobuf", "c-ares", "benchmark",
              "grpc", "gtest", "crc32c", "curl",
              "nlohmann-json")

$project_root = (Get-Item -Path ".\" -Verbose).FullName -replace "\\", "/"
$vcpkg_root = Install-Vcpkg "${project_root}/cmake-out" "${BuildName}"
Build-Vcpkg-Packages $vcpkg_root @("benchmark", "crc32c", "curl", "grpc", "gtest", "nlohmann-json", "openssl", "protobuf")

$binary_dir="cmake-out/${BuildName}"
$cmake_args=@(
    "-G$env:GENERATOR",
    "-S", ".",
    "-B", "${binary_dir}"
    "-DCMAKE_TOOLCHAIN_FILE=`"${vcpkg_root}/scripts/buildsystems/vcpkg.cmake`""
    "-DCMAKE_BUILD_TYPE=${env:CONFIG}",
    "-DVCPKG_TARGET_TRIPLET=${env:VCPKG_TRIPLET}",
    "-DCMAKE_C_COMPILER=cl.exe",
    "-DCMAKE_CXX_COMPILER=cl.exe"
)

# Configure CMake and create the build directory.
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Configuring CMake with $cmake_args"
cmake $cmake_args
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

$ctest_args = @(
    "--output-on-failure",
    "-j", $NCPU,
    "-C", $env:CONFIG,
    "--progress"
)
ctest $ctest_args -LE integration-test
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "ctest failed with exit code $LastExitCode"
    Exit ${LastExitCode}
}

# Import the functions and variables used to run integration tests
Set-Location "${project_root}"
. ci/kokoro/windows/lib/integration.ps1

if (Test-Integration-Enabled) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Running minimal quickstart programs $env:CONFIG"
    Install-Roots-Pem
    ${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}="${env:KOKORO_GFILE_DIR}/roots.pem"
    ${env:GOOGLE_APPLICATION_CREDENTIALS}="${env:KOKORO_GFILE_DIR}/kokoro-run-key.json"
    Set-Location "${binary_dir}"
    ctest $ctest_args -R "(storage_quickstart|pubsub_quickstart)"
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "ctest failed with exit code $LastExitCode"
        Exit ${LastExitCode}
    }
    Set-Location "${project_root}"
}

Write-Host -ForegroundColor Green "`n$(Get-Date -Format o) DONE"
