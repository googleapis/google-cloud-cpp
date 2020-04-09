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
ForEach($var in ("CONFIG", "GENERATOR", "VCPKG_TRIPLET")) {
    if (-not (Test-Path env:${var})) {
        $missing+=(${var})
    }
}
if ($missing.count -ge 1) {
    throw "Aborting build because the ${missing} environment variables are not set."
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
    throw "cmake config failed with exit code $LastExitCode"
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling with CMake $env:CONFIG"
cmake --build "${binary_dir}" --config $env:CONFIG
if ($LastExitCode) {
    throw "cmake for 'all' target failed with exit code $LastExitCode"
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
ctest $ctest_flags -LE integration-tests
if ($LastExitCode) {
    throw "ctest failed with exit code $LastExitCode"
}

if ((Test-Path env:RUN_INTEGRATION_TESTS) -and ($env:RUN_INTEGRATION_TESTS -eq "true")) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Running integration tests $env:CONFIG"
    ctest $ctest_flags -L integration-tests
    if ($LastExitCode) {
        throw "Integration tests failed with exit code $LastExitCode"
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) DONE"
