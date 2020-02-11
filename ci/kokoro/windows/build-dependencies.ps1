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

$project_root = (Get-Item -Path ".\" -Verbose).FullName

# First check the required environment variables.
if (-not (Test-Path env:CONFIG)) {
    throw "Aborting build because the CONFIG environment variable is not set."
}
$CONFIG = $env:CONFIG

# Set BUILD_CACHE, defauting to the original value
if (-not (Test-Path env:BUILD_CACHE)) {
    $BUILD_CACHE = "gs://cloud-cpp-kokoro-results/" +
        "build-artifacts/vcpkg-installed.zip"
} else {
    $BUILD_CACHE = $env:BUILD_CACHE
}

# Update or clone the 'vcpkg' package manager, this is a bit overly complicated,
# but it works well on your workstation where you may want to run this script
# multiple times while debugging vcpkg installs.  It also works on AppVeyor
# where we cache the vcpkg installation, but it might be empty on the first
# build.
Write-Host "Obtaining vcpkg repository."
Get-Date -Format o
Set-Location ..
if (Test-Path vcpkg\.git) {
    Set-Location vcpkg
    git pull
} elseif (Test-Path vcpkg\installed) {
    Move-Item vcpkg vcpkg-tmp
    git clone https://github.com/Microsoft/vcpkg
    Move-Item vcpkg-tmp\installed vcpkg
    Set-Location vcpkg
} else {
    git clone https://github.com/Microsoft/vcpkg
    Set-Location vcpkg
}
if ($LastExitCode) {
    throw "vcpkg git setup failed with exit code $LastExitCode"
}

Write-Host "Downloading build cache."
Get-Date -Format o
gsutil cp $BUILD_CACHE vcpkg-installed.zip
if ($LastExitCode) {
    # Ignore errors, caching failures should not break the build.
    Write-Host "gsutil download failed with exit code $LastExitCode"
}

Write-Host "Extracting build cache."
Get-Date -Format o
C:\Windows\system32\cmd /c 7z x vcpkg-installed.zip -aoa
if ($LastExitCode) {
    # Ignore errors, caching failures should not break the build.
    Write-Host "extracting build cache failed with exit code $LastExitCode"
}

Write-Host "Bootstrap vcpkg."
Get-Date -Format o
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

$vcpkg_flags=@(
    "--triplet", "x64-windows-static",
    "--overlay-ports=${project_root}/ci/kokoro/windows/vcpkg-ports")

    # Remove old versions of the packages.
Write-Host "Cleanup old vcpkg package versions."
Get-Date -Format o
.\vcpkg.exe remove ${vcpkg_flags} --outdated --recurse
if ($LastExitCode) {
    throw "vcpkg remove --outdated failed with exit code $LastExitCode"
}

Write-Host "Building vcpkg package versions."
Get-Date -Format o
$packages = @("zlib", "openssl",
              "protobuf", "c-ares",
              "grpc", "curl",
              "gtest", "crc32c"
              "googleapis", "google-cloud-cpp-common")
foreach ($pkg in $packages) {
    .\vcpkg.exe install ${vcpkg_flags} ${pkg}
    if ($LastExitCode) {
        throw "vcpkg install $pkg failed with exit code $LastExitCode"
    }
}

Write-Host "================================================================"
Write-Host "================================================================"
.\vcpkg.exe list

Write-Host "================================================================"
Write-Host "================================================================"
Write-Host "Create cache zip file."
Get-Date -Format o
C:\Windows\system32\cmd /c 7z a vcpkg-installed.zip installed\
if ($LastExitCode) {
    # Ignore errors, caching failures should not break the build.
    Write-Host "zip build cache failed with exit code $LastExitCode"
}

Write-Host "================================================================"
Write-Host "================================================================"
Write-Host "Upload cache zip file."
Get-Date -Format o
gsutil cp vcpkg-installed.zip $BUILD_CACHE
if ($LastExitCode) {
    # Ignore errors, caching failures should not break the build.
    Write-Host "gsutil upload failed with exit code $LastExitCode"
}
