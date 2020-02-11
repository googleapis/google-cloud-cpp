# !/usr/bin/env powershell

# Copyright 2018 Google LLC
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
if (-not (Test-Path env:CONFIG)) {
    throw "Aborting build because the CONFIG environment variable is not set."
}

$CONFIG = $env:CONFIG
$GENERATOR = "Ninja"

# Set TEMP explicitly for windows 2019 image
$Env:TEMP = "T:\tmp\"

# By default assume "module", use the configuration parameters and build in the `cmake-out` directory.
$cmake_flags=@("-G$GENERATOR", "-DCMAKE_BUILD_TYPE=$CONFIG", "-H.", "-Bcmake-out")

# This script expects vcpkg to be installed in ..\vcpkg, discover the full
# path to that directory:
$cwd = (Get-Item -Path ".\" -Verbose).FullName
$dir = Split-Path "$cwd"

# Setup the environment for vcpkg:
$cmake_flags += "-DCMAKE_TOOLCHAIN_FILE=`"$dir\vcpkg\scripts\buildsystems\vcpkg.cmake`""
$cmake_flags += "-DVCPKG_TARGET_TRIPLET=x64-windows-static"
$cmake_flags += "-DCMAKE_C_COMPILER=cl.exe"
$cmake_flags += "-DCMAKE_CXX_COMPILER=cl.exe"

# Configure CMake and create the build directory.
Write-Host
Get-Date -Format o
Write-Host "Configuring CMake with $cmake_flags"
cmake $cmake_flags
if ($LastExitCode) {
    throw "cmake config failed with exit code $LastExitCode"
}

Write-Host
Get-Date -Format o
Write-Host "Compiling with CMake $env:CONFIG"
cmake --build cmake-out --config $CONFIG
if ($LastExitCode) {
    throw "cmake for 'all' target failed with exit code $LastExitCode"
}

Write-Host
Get-Date -Format o
Set-Location cmake-out
ctest --output-on-failure -C $CONFIG
if ($LastExitCode) {
    throw "ctest failed with exit code $LastExitCode"
}

Write-Host
Get-Date -Format o
