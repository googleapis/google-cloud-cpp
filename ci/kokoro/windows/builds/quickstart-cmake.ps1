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

# Load the functions to configure and use CMake.
. ci/kokoro/windows/lib/cmake.ps1

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

$project_root = (Get-Item -Path ".\" -Verbose).FullName -replace "\\", "/"
$vcpkg_root = Install-Vcpkg "${project_root}" "-qs"
$binary_dir="cmake-out/${BuildName}"
Build-Vcpkg-Packages $vcpkg_root @("google-cloud-cpp")

$cmake_args=@(
    "-G$env:GENERATOR",
    "-S", "google/cloud/storage/quickstart",
    "-B", "${binary_dir}/storage"
    "-DCMAKE_TOOLCHAIN_FILE=`"${vcpkg_root}/scripts/buildsystems/vcpkg.cmake`""
    "-DCMAKE_BUILD_TYPE=${env:CONFIG}",
    "-DVCPKG_TARGET_TRIPLET=${env:VCPKG_TRIPLET}",
    "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
    "-DCMAKE_C_COMPILER=cl.exe",
    "-DCMAKE_CXX_COMPILER=cl.exe"
)

Write-Host "`n$(Get-Date -Format o) Configuring CMake with $cmake_args"
cmake $cmake_args

Write-Host "`n$(Get-Date -Format o) Compiling $target with CMake $env:CONFIG"
cmake --build "${binary_dir}/quickstart-storage" --config $env:CONFIG

Write-Host -ForegroundColor Green "`n$(Get-Date -Format o) DONE"
