#!/usr/bin/env powershell

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

# Stop on errors. This has a similar effect as `set -e` on Unix shells.
$ErrorActionPreference = "Stop"

# First check if the compilation is setup to use modules.
if (-not (Test-Path env:PROVIDER)) {
    throw "Aborting build because the PROVIDER environment variable is not set."
}

# If this is a build using the submodules just install nasm.  The rest of this script configures vcpkg, which
# is not used in this case.
if ($env:PROVIDER -eq "module") {
    choco install -y nasm
    return
}

# BEGIN WORKAROUND
# As of 2018-08-30 AppVeyor had a broken image, reportedly this is a workaround.
#     https://github.com/Microsoft/vcpkg/issues/4189#issuecomment-417462822
Remove-Item C:\OpenSSL-v11-Win32 -Recurse -Force -ErrorAction Ignore
Remove-Item C:\OpenSSL-v11-Win64 -Recurse -Force -ErrorAction Ignore
# END WORKAROUND

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
    throw "git setup failed with exit code $LastExitCode"
}

# Build the tool each time, it is fast to do so.
powershell -exec bypass scripts\bootstrap.ps1
if ($LastExitCode) {
    throw "vcpkg bootstrap failed with exit code $LastExitCode"
}

# Integrate installed packages into the build environment.
& .\vcpkg.exe integrate install
if ($LastExitCode) {
    throw "vcpkg integrate failed with exit code $LastExitCode"
}

# Remove old versions of the packages.
& .\vcpkg.exe remove --outdated --recurse
if ($LastExitCode) {
    throw "vcpkg remove --outdated failed with exit code $LastExitCode"
}

# AppVeyor limits builds to 60 minutes. Building all the dependencies takes
# longer than that. Cache the dependencies to work around the build time
# restrictions. Explicitly install each dependency because if we run out of
# time in the AppVeyor build the cache is at least partially refreshed, a
# rebuild will start with some dependencies already cached, and likely
# complete before the AppVeyor build time limit.
$packages = @("zlib:x64-windows-static", "openssl:x64-windows-static",
              "protobuf:x64-windows-static", "c-ares:x64-windows-static",
              "grpc:x64-windows-static", "curl:x64-windows-static",
              "gtest:x64-windows-static", "crc32c:x64-windows-static")
foreach ($pkg in $packages) {
    .\vcpkg.exe install $pkg
    if ($LastExitCode) {
        throw "vcpkg install $pkg failed with exit code $LastExitCode"
    }
}
