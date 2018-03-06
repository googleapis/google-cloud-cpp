# !/usr/bin/env powershell

# Copyright 2017 Google Inc.
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
if (-not (Test-Path env:PROVIDER)) {
    throw "Aborting build because the PROVIDER environment variable is not set."
}
if (-not (Test-Path env:GENERATOR)) {
    throw "Aborting build because the GENERATOR environment variable is not set."
}
if (-not (Test-Path env:CONFIG)) {
    throw "Aborting build because the CONFIG environment variable is not set."
}

# By default assume "module", use the configuration parameters and build in the `build-output` directory.
$cmake_flags=@("-G$env:GENERATOR", "-DCMAKE_BUILD_TYPE=$env:CONFIG", "-H.", "-Bbuild-output")

if ($env:PROVIDER -eq "vcpkg") {
    # Setup the environment for vcpkg:
    $dir = Split-Path (Get-Item -Path ".\" -Verbose).FullName
    if (Test-Path variable:env:APPVEYOR_BUILD_FOLDER) {
        $dir = Split-Path $env:APPVEYOR_BUILD_FOLDER
    }

    $integrate = "$dir\vcpkg\vcpkg.exe integrate install"
    Invoke-Expression $integrate
    if ($LastExitCode) {
        throw "vcpkg integrate failed with exit code $LastExitCode"
    }

    $cmake_flags += "-DGOOGLE_CLOUD_CPP_GRPC_PROVIDER=$env:PROVIDER"
    $cmake_flags += "-DCMAKE_TOOLCHAIN_FILE=`"$dir\vcpkg\scripts\buildsystems\vcpkg.cmake`""
    $cmake_flags += "-DVCPKG_TARGET_TRIPLET=x64-windows-static"
}

# Create a build directory, and run cmake in it.
cmake $cmake_flags
if ($LastExitCode) {
    throw "cmake failed with exit code $LastExitCode"
}

# Compile inside the build directory. Pass /m flag to msbuild to use all cores.
cmake --build build-output -- /m
if ($LastExitCode) {
    throw "cmake build failed with exit code $LastExitCode"
}
