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

# If possible, configure the vcpkg cache.
. ci/kokoro/windows/lib/vcpkg-cache.ps1

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

$project_root = (Get-Item -Path ".\" -Verbose).FullName
$vcpkg_version = Get-Content -Path "${project_root}\ci\etc\vcpkg-commit.txt"
$vcpkg_base = "vcpkg"
$packages = @("zlib", "openssl",
              "protobuf", "c-ares", "benchmark",
              "grpc", "gtest", "crc32c", "curl",
              "nlohmann-json")
$vcpkg_flags=@(
    "--triplet", "${env:VCPKG_TRIPLET}")
if ($args.count -ge 1) {
    $vcpkg_base, $packages = $args
    $vcpkg_flags=("--triplet", "${env:VCPKG_TRIPLET}")
}
$vcpkg_dir = "cmake-out\${vcpkg_base}"
$Env:VCPKG_FEATURE_FLAGS = "-manifests"

New-Item -ItemType Directory -Path "cmake-out" -ErrorAction SilentlyContinue
# Download the right version of `vcpkg`
if (Test-Path "${vcpkg_dir}") {
    Write-Host -ForegroundColor Green "`n$(Get-Date -Format o) vcpkg directory already exists."
} else {
    ForEach($_ in (1, 2, 3)) {
        if ( $_ -ne 1) {
            Start-Sleep -Seconds (60 * $_)
        }
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Downloading vcpkg ports archive [$_]"
        try {
            (New-Object System.Net.WebClient).Downloadfile(
                    "https://github.com/microsoft/vcpkg/archive/${vcpkg_version}.zip",
                    "cmake-out\${vcpkg_version}.zip")
            break
        } catch {
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) download error"
        }
    }
    7z x -ocmake-out "cmake-out\${vcpkg_version}.zip" -aoa -bsp0
    if ($LastExitCode) {
        # Ignore errors, caching failures should not break the build.
        Write-Host -ForegroundColor Red "`n$(Get-Date -Format o) " `
            "extracting vcpkg from archive failed with exit code ${LastExitCode}."
        Exit 1
    }
    Set-Location "cmake-out"
    Rename-Item "vcpkg-${vcpkg_version}" "${vcpkg_base}"
    Set-Location ".."
}
if (-not (Test-Path "${vcpkg_dir}")) {
    Write-Host -ForegroundColor Red "Missing vcpkg directory (${vcpkg_dir})."
    Exit 1
}

# On Windows the bootstrap script simply downloads the latest release, so this
# is not very slow:
ForEach($_ in (1, 2, 3)) {
    Write-Host -ForegroundColor Green "`n$(Get-Date -Format o) bootstrap vcpkg."
    if (Test-Path "${vcpkg_dir}\vcpkg.exe") {
        break
    }
    try {
        &"${vcpkg_dir}/bootstrap-vcpkg.bat"
        if (Test-Path "${vcpkg_dir}\vcpkg.exe") {
            break
        }
    } catch {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) bootstrap error"
    }
    Start-Sleep -Seconds (60 * $_)
}
if (-not (Test-Path "${vcpkg_dir}\vcpkg.exe")) {
    Write-Host -ForegroundColor Red "Missing vcpkg executable (${vcpkg_dir}\vcpkg.exe)."
    Exit 1
}

# Remove old versions of the packages.
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Cleanup outdated vcpkg packages."
&"${vcpkg_dir}\vcpkg.exe" remove ${vcpkg_flags} --outdated --recurse
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "vcpkg remove --outdated failed with exit code $LastExitCode"
    Exit ${LastExitCode}
}

ForEach($_ in (1, 2, 3)) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Warmup vcpkg [$_]"
    &"${vcpkg_dir}\vcpkg.exe" install ${vcpkg_flags} "crc32c"
    if ($LastExitCode -eq 0) {
        break
    }
    Start-Sleep -Seconds (60 * $_)
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Building vcpkg packages."
foreach ($pkg in $packages) {
    &"${vcpkg_dir}\vcpkg.exe" install ${vcpkg_flags} "${pkg}"
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "vcpkg install $pkg failed with exit code $LastExitCode"
        Exit ${LastExitCode}
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) vcpkg list"
&"${vcpkg_dir}\vcpkg.exe" list

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Disk(s) size and space for troubleshooting"
Get-CimInstance -Class CIM_LogicalDisk | `
    Select-Object -Property DeviceID, DriveType, VolumeName, `
        @{L='FreeSpaceGB';E={"{0:N2}" -f ($_.FreeSpace /1GB)}}, `
        @{L="Capacity";E={"{0:N2}" -f ($_.Size/1GB)}}
