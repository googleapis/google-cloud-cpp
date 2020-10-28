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
    Write-Host -ForegroundColor Red `
        "Aborting build because the CONFIG environment variable is not set."
    Exit 1
}
if (-not (Test-Path env:VCPKG_TRIPLET)) {
    Write-Host -ForegroundColor Red `
        "Aborting build because the VCPKG_TRIPLET environment variable is not set."
    Exit 1
}

$RunningCI = (Test-Path env:RUNNING_CI) -and `
    ($env:RUNNING_CI -eq "yes")
$IsCI = (Test-Path env:KOKORO_JOB_TYPE) -and `
    ($env:KOKORO_JOB_TYPE -eq "CONTINUOUS_INTEGRATION")
$IsPR = (Test-Path env:KOKORO_JOB_TYPE) -and `
    ($env:KOKORO_JOB_TYPE -eq "PRESUBMIT_GITHUB")
$HasBuildCache = (Test-Path env:BUILD_CACHE)

$vcpkg_dir = "cmake-out\vcpkg"
$packages = @("zlib", "openssl",
              "protobuf", "c-ares", "benchmark",
              "grpc", "gtest", "crc32c", "curl",
              "nlohmann-json")
$vcpkg_flags=@(
    "--triplet", "${env:VCPKG_TRIPLET}")
if ($args.count -ge 1) {
    $vcpkg_dir, $packages = $args
    $vcpkg_flags=("--triplet", "${env:VCPKG_TRIPLET}")
}

# Update or clone the 'vcpkg' package manager, this is a bit overly complicated,
# but it works well on your workstation where you may want to run this script
# multiple times while debugging vcpkg installs.  It also works on Kokoro
# where we cache the vcpkg installation, but it might be empty on the first
# build.
if ($RunningCI -and (Test-Path "${vcpkg_dir}")) {
    Remove-Item -LiteralPath "${vcpkg_dir}" -Force -Recurse
}
if (-not (Test-Path ${vcpkg_dir})) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Cloning vcpkg repository."
    git clone --quiet --shallow-since 2020-10-20 `
        https://github.com/microsoft/vcpkg.git "${vcpkg_dir}"
    git -C "${vcpkg_dir}" checkout 79e72302cdbbf8ca646879b13cd84ebd68707abf
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red `
            "vcpkg git setup failed with exit code $LastExitCode"
        Exit ${LastExitCode}
    }
} else {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Updating vcpkg repository."
    git -C "${vcpkg_dir}" pull
    git -C "${vcpkg_dir}" checkout 79e72302cdbbf8ca646879b13cd84ebd68707abf
}
if (-not (Test-Path "${vcpkg_dir}")) {
    Write-Host -ForegroundColor Red "Missing vcpkg directory (${vcpkg_dir})."
    Exit 1
}
Set-Location "${vcpkg_dir}"

# If BUILD_CACHE is set (which typically is on Kokoro builds), try
# to download and extract the build cache.
if ($RunningCI -and $HasBuildCache) {
    gcloud auth activate-service-account `
        --key-file "${env:KOKORO_GFILE_DIR}/build-results-service-account.json"
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
        "downloading build cache."
    gsutil -q cp ${env:BUILD_CACHE} vcpkg-installed.zip
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
  Write-Host -ForegroundColor Red "Error bootstrapping vcpkg: $LastExitCode"
  Exit ${LastExitCode}
}

# Integrate installed packages into the build environment.
.\vcpkg.exe integrate install
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "vcpkg integrate failed with exit code $LastExitCode"
    Exit ${LastExitCode}
}

# Remove old versions of the packages.
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Cleanup old vcpkg package versions."
.\vcpkg.exe remove ${vcpkg_flags} --outdated --recurse
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "vcpkg remove --outdated failed with exit code $LastExitCode"
    Exit ${LastExitCode}
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Building vcpkg package versions."
foreach ($pkg in $packages) {
    .\vcpkg.exe install ${vcpkg_flags} "${pkg}"
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "vcpkg install $pkg failed with exit code $LastExitCode"
        Exit ${LastExitCode}
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) vcpkg list"
.\vcpkg.exe list

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Cleanup vcpkg buildtrees"
Get-ChildItem -Recurse -File `
        -ErrorAction SilentlyContinue `
        -Path "buildtrees" | `
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Disk(s) size and space for troubleshooting"
    Get-CimInstance -Class CIM_LogicalDisk | `
        Select-Object -Property DeviceID, DriveType, VolumeName, `
            @{L='FreeSpaceGB';E={"{0:N2}" -f ($_.FreeSpace /1GB)}}, `
            @{L="Capacity";E={"{0:N2}" -f ($_.Size/1GB)}}

# Do not update the vcpkg cache on PRs, it might dirty the cache for any
# PRs running in parallel, and it is a waste of time in most cases.
if ($RunningCI -and $IsCI -and $HasBuildCache) {
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
        gcloud auth activate-service-account `
           --key-file "${env:KOKORO_GFILE_DIR}/build-results-service-account.json"
        gsutil -q cp vcpkg-installed.zip "${env:BUILD_CACHE}"
        if ($LastExitCode) {
            # Ignore errors, caching failures should not break the build.
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
                "continue despite upload failure with code ${LastExitCode}".
        }
    }
} else {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
      "vcpkg not updated IsCI = $IsCI, IsPR = $IsPR, " `
      "HasBuildCache = $HasBuildCache."
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Disk(s) size and space for troubleshooting"
Get-CimInstance -Class CIM_LogicalDisk | `
    Select-Object -Property DeviceID, DriveType, VolumeName, `
        @{L='FreeSpaceGB';E={"{0:N2}" -f ($_.FreeSpace /1GB)}}, `
        @{L="Capacity";E={"{0:N2}" -f ($_.Size/1GB)}}
