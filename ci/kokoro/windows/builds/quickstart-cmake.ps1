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

function Get-Vcpkg-Features {
    param([string]$vcpkg_root, [string]$package)

    & "${vcpkg_root}/vcpkg.exe" search "${package}" | 
        Where-Object { $_.StartsWith("${package}[") } |
        ForEach-Object { $_.replace("${package}[", "") -replace "].*", "" } |
        Where-Object { -not (
            # TODO(#8145) - does not compile on Windows.
            "asset",
            # TODO(#8125) - does not compile on Windows.
            "channel",
            # TODO(#8725) - does not compile on Windows.
            "storagetransfer" -contains $_) } |
        # TODO(#9913) - these compile, but do not install on Windows.
        Where-Object { -not ("assuredworkloads",  "dialogflow-cx", "dialogflow-es" -contains $_) } |
        # TODO(#9914) - these depends on the `grafeas`, feature but vcpkg does not install it
        Where-Object { -not ( "binaryauthorization", "containeranalysis" -contains $_) } |
        # These are convenience features to refactor dependencies; they do not have quickstarts.
        Where-Object { -not ("googleapis", "grpc-common", "grafeas" -contains $_) }
}

$project_root = (Get-Item -Path ".\" -Verbose).FullName -replace "\\", "/"
$vcpkg_root = Install-Vcpkg "${project_root}" "-qs"
$binary_dir="cmake-out/${BuildName}"
$features = Get-Vcpkg-Features "${vcpkg_root}" "google-cloud-cpp"
Build-Vcpkg-Packages $vcpkg_root @("google-cloud-cpp[$($features  -Join ',')]")

$failures=@()
ForEach($feature in $features) {
    if ($feature -eq "experimental-storage-grpc") {
        # Skip this feature as it is not a separate library with a quickstart.
        continue
    }
    $library = $feature.replace('-', '_')
    Write-Host "`n$(Get-Date -Format o) Build quickstart for ${library}"
    $cmake_args=@(
        "-G$env:GENERATOR",
        "-S", "google/cloud/${library}/quickstart",
        "-B", "${binary_dir}/${library}"
        "-DCMAKE_TOOLCHAIN_FILE=`"${vcpkg_root}/scripts/buildsystems/vcpkg.cmake`""
        "-DCMAKE_BUILD_TYPE=${env:CONFIG}",
        "-DVCPKG_TARGET_TRIPLET=${env:VCPKG_TRIPLET}",
        "-DCMAKE_CXX_COMPILER=cl.exe"
    )

    Write-Host "$(Get-Date -Format o) Configuring CMake with $cmake_args"
    cmake $cmake_args
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "CMake configuration failed with ${LastExitCode}."
        $failures += (${library})
        continue
    }
    Write-Host "$(Get-Date -Format o) Compiling $target with CMake $env:CONFIG"
    cmake --build "${binary_dir}/${library}" --config $env:CONFIG
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "CMake configuration failed with ${LastExitCode}."
        $failures += (${library})
    }
}

if ($failures.count -eq 0) {
    Write-Host -ForegroundColor Green "`n$(Get-Date -Format o) All quickstarts built successfully"
    Exit 0
}

Write-Host -ForegroundColor Red "`n$(Get-Date -Format o) The following quickstarts failed to build: ${failures}"
Exit 1
