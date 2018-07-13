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
if (-not (Test-Path env:CONFIG)) {
    throw "Aborting build because the CONFIG environment variable is not set."
}
$CONFIG = $env:CONFIG

# Update or clone the 'vcpkg' package manager, this is a bit overly complicated,
# but it works well on your workstation where you may want to run this script
# multiple times while debugging vcpkg installs.  It also works on AppVeyor
# where we cache the vcpkg installation, but it might be empty on the first
# build.
cd ..
if (Test-Path vcpkg\.git) {
    cd vcpkg
    git pull
} elseif (Test-Path vcpkg\installed) {
    move vcpkg vcpkg-tmp
    git clone https://github.com/Microsoft/vcpkg
    move vcpkg-tmp\installed vcpkg
    cd vcpkg
} else {
    git clone https://github.com/Microsoft/vcpkg
    cd vcpkg
}
if ($LastExitCode) {
    throw "vcpkg git setup failed with exit code $LastExitCode"
}

powershell -exec bypass scripts\bootstrap.ps1
if ($LastExitCode) {
    throw "vcpkg bootstrap failed with exit code $LastExitCode"
}

# Only compile the release version of the packages we need, for Debug builds
# this trick does not work, so we need to compile all versions (yuck).
if ($CONFIG -eq "Release") {
    Add-Content triplets\x64-windows-static.cmake "set(VCPKG_BUILD_TYPE release)"
}

# Integrate installed packages into the build environment.
.\vcpkg.exe integrate install
if ($LastExitCode) {
    throw "vcpkg integrate failed with exit code $LastExitCode"
}

.\vcpkg.exe remove --outdated
if ($LastExitCode) {
    throw "vcpkg remove outdated failed with exit code $LastExitCode"
}

# Build the dependencies that we cannot use as external packages.
$packages = @("curl:x64-windows-static", "grpc:x64-windows-static",
              "gtest:x64-windows-static")
foreach ($pkg in $packages) {
    .\vcpkg.exe install $pkg
    if ($LastExitCode) {
        throw "vcpkg install $pkg failed with exit code $LastExitCode"
    }
}
