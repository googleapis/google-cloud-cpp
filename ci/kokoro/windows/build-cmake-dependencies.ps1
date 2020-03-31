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
if (-not (Test-Path env:CONFIG)) {
    throw "Aborting build because the CONFIG environment variable is not set."
}
$CONFIG = $env:CONFIG

$project_root = (Get-Item -Path ".\" -Verbose).FullName

# Update or clone the 'vcpkg' package manager, this is a bit overly complicated,
# but it works well on your workstation where you may want to run this script
# multiple times while debugging vcpkg installs.  It also works on AppVeyor
# where we cache the vcpkg installation, but it might be empty on the first
# build.
Set-Location ..
if (Test-Path env:RUNNING_CI) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Cloning vcpkg repository."
    if (Test-Path "vcpkg") {
        Remove-Item -LiteralPath "vcpkg" -Force -Recurse
    }
    git clone --quiet --depth 10 https://github.com/Microsoft/vcpkg.git
    if ($LastExitCode) {
      throw "vcpkg git setup failed with exit code $LastExitCode"
    }
}
if (-not (Test-Path "vcpkg")) {
    throw "Missing vcpkg directory."
}
Set-Location vcpkg

# If BUILD_CACHE is set (which typically is on Kokoro builds), try
# to download and extract the build cache.
if (Test-Path env:BUILD_CACHE) {
    gcloud auth activate-service-account `
        --key-file "${env:KOKORO_GFILE_DIR}/build-results-service-account.json"
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
        "downloading build cache."
    gsutil -q cp $env:BUILD_CACHE vcpkg-installed.zip
    if ($LastExitCode) {
        # Ignore errors, caching failures should not break the build.
        Write-Host "gsutil download failed with exit code $LastExitCode"
    } else {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "extracting build cache."
        7z x vcpkg-installed.zip -aoa -bsp0
        if ($LastExitCode) {
            # Ignore errors, caching failures should not break the build.
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
                "extracting build cache failed with exit code ${LastExitCode}."
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
                "build will continue without a cache."
        }
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Bootstrap vcpkg."
powershell -exec bypass scripts\bootstrap.ps1
if ($LastExitCode) {
  throw "Error bootstrapping vcpkg: $LastExitCode"
  Write-Host -ForegroundColor Red "bootstrap[1] failed"
}

# Only compile the release version of the packages we need, for Debug builds
# this trick does not work, so we need to compile all versions (yuck).
if ($CONFIG -eq "Release") {
#    Add-Content triplets\x64-windows-static.cmake "set(VCPKG_BUILD_TYPE release)"
}

# Integrate installed packages into the build environment.
.\vcpkg.exe integrate install
if ($LastExitCode) {
    throw "vcpkg integrate failed with exit code $LastExitCode"
}

$vcpkg_flags=@(
    "--triplet", "${env:VCPKG_TRIPLET}",
    "--overlay-ports=${project_root}/ci/kokoro/windows/vcpkg-ports")

# Remove old versions of the packages.
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Cleanup old vcpkg package versions."
.\vcpkg.exe remove ${vcpkg_flags} --outdated --recurse
if ($LastExitCode) {
    throw "vcpkg remove --outdated failed with exit code $LastExitCode"
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Building vcpkg package versions."
$packages = @("zlib", "openssl",
              "protobuf", "c-ares",
              "grpc", "gtest", "crc32c", "curl",
              "googleapis", "google-cloud-cpp-common[test]")
foreach ($pkg in $packages) {
    .\vcpkg.exe install ${vcpkg_flags} "${pkg}"
    if ($LastExitCode) {
        throw "vcpkg install $pkg failed with exit code $LastExitCode"
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) vcpkg list"
.\vcpkg.exe list

# Do not update the vcpkg cache on PRs, it might dirty the cache for any
# PRs running in parallel, and it is a waste of time in most cases.
$IsPR = (Test-Path env:KOKORO_JOB_TYPE) -and `
    ($env:KOKORO_JOB_TYPE -eq "GITHUB_PULL_REQUEST")
$HasBuildCache = (Test-Path env:BUILD_CACHE)
if ((-not $IsPR) -and $HasBuildCache) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
      "zip vcpkg cache for upload."
    7z a vcpkg-installed.zip installed\ -bsp0
    if ($LastExitCode) {
        # Ignore errors, caching failures should not break the build.
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "zip failed with exit code ${LastExitCode}."
    } else {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "upload zip with vcpkg cache."
        gsutil -q cp vcpkg-installed.zip "${env:BUILD_CACHE}"
        if ($LastExitCode) {
            # Ignore errors, caching failures should not break the build.
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
                "continue despite upload failure with code ${LastExitCode}".
        }
    }
} else {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
      "vcpkg not updated IsPR == $IsPR, HasBuildCache == $HasBuildCache."
}
