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

$project_root = (Get-Item -Path ".\" -Verbose).FullName -replace "\\", "/"
$vcpkg_root = Install-Vcpkg "${project_root}" ""

Write-Host -ForegroundColor Cyan "Patching crc32c portfile for CMake policy..."
$crc32c_portfile = "${vcpkg_root}/ports/crc32c/portfile.cmake"
if (Test-Path $crc32c_portfile) {
    # --- DEBUG LOGGING (BEFORE) ---
    Write-Host -ForegroundColor Yellow "========= CONTENTS of $crc32c_portfile BEFORE patch ========="
    Get-Content $crc32c_portfile
    Write-Host -ForegroundColor Yellow "============================================================"
    # --- END DEBUG LOGGING ---

    $content = Get-Content -Raw -Path $crc32c_portfile

    # Add our policy fix to the vcpkg_cmake_configure call
    # Use the .Replace() method for a literal string replacement, not regex
    $new_content = $content.Replace("vcpkg_cmake_configure(", "vcpkg_cmake_configure( OPTIONS -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ")

    # --- DEBUG LOGGING (AFTER) ---
    Write-Host -ForegroundColor Yellow "========= CONTENTS of $crc32c_portfile AFTER patch ========="
    Write-Host $new_content
    Write-Host -ForegroundColor Yellow "==========================================================="
    # --- END DEBUG LOGGING ---

    Set-Content -Path $crc32c_portfile -Value $new_content
    Write-Host -ForegroundColor Cyan "Patch applied to $crc32c_portfile."
} else {
    Write-Host -ForegroundColor Red "Could not find crc32c portfile at $crc32c_portfile. Skipping patch (build will likely fail)."
}

$binary_dir="cmake-out/${BuildName}"
# Install all dependencies from the vcpkg.json manifest file.
# This mirrors the behavior of our GHA builds.
Write-Host -ForegroundColor Yellow "Attempting vcpkg install..."
& "${vcpkg_root}/vcpkg.exe" install --triplet "${env:VCPKG_TRIPLET}"

# Manually check the exit code. vcpkg might not be throwing a terminating error.
if ($LastExitCode -ne 0) {
    Write-Host -ForegroundColor Red "----------------------------------------------------------------"
    Write-Host -ForegroundColor Red "vcpkg install FAILED with exit code $LastExitCode."
    Write-Host -ForegroundColor Red "Dumping vcpkg buildtree logs for crc32c..."
    Write-Host -ForegroundColor Red "----------------------------------------------------------------"

    # Define the log files based on the error message
    $log1 = "${vcpkg_root}/buildtrees/crc32c/config-x64-windows-static-out.log"
    $log2 = "${vcpkg_root}/buildtrees/crc32c/config-x64-windows-static-dbg-CMakeCache.txt.log"
    $log3 = "${vcpkg_root}/buildtrees/crc32c/config-x64-windows-static-rel-CMakeCache.txt.log"

    foreach ($logFile in @($log1, $log2, $log3)) {
        if (Test-Path $logFile) {
            Write-Host -ForegroundColor Red "========= Contents of $logFile ========="
            Get-Content $logFile
            Write-Host -ForegroundColor Red "========= End of $logFile ========="
        } else {
            Write-Host -ForegroundColor Yellow "Log file not found, skipping: $logFile"
        }
    }

    Write-Host -ForegroundColor Red "----------------------------------------------------------------"
    Write-Host -ForegroundColor Red "Dumping complete. Forcing build failure."
    Write-Host -ForegroundColor Red "----------------------------------------------------------------"
    # Manually fail the build with the exit code from vcpkg
    exit $LastExitCode
}

Write-Host -ForegroundColor Green "vcpkg install SUCCEEDED."

$cmake_args=@(
    "-G$env:GENERATOR",
    "-S", ".",
    "-B", "${binary_dir}"
    "-DCMAKE_TOOLCHAIN_FILE=`"${vcpkg_root}/scripts/buildsystems/vcpkg.cmake`""
    "-DCMAKE_BUILD_TYPE=${env:CONFIG}",
    "-DVCPKG_TARGET_TRIPLET=${env:VCPKG_TRIPLET}",
    "-DCMAKE_C_COMPILER=cl.exe",
    "-DCMAKE_CXX_COMPILER=cl.exe",
    "-DGOOGLE_CLOUD_CPP_ENABLE_WERROR=ON",
    "-DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND=ON",
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>",
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
)

# Configure CMake and create the build directory.
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Configuring CMake with $cmake_args"
cmake $cmake_args
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "cmake config failed with exit code $LastExitCode"
    Get-Content -Path "${binary_dir}/vcpkg-manifest-install.log"
    Exit ${LastExitCode}
}

# Workaround some flaky / broken tests in the CI builds, some of the
# tests where (reported) successfully created during the build,
# only to be missing when running the tests. This is probably a toolchain
# bug, and this seems to workaround it.
$workaround_targets=(
    # Failed around 2020-07-29
    "storage_internal_tuple_filter_test",
    # Failed around 2020-08-10
    "storage_well_known_parameters_test",
    # Failed around 2021-01-25
    "common_internal_random_test",
    "common_future_generic_test",
    "googleapis_download"
)
ForEach($target IN $workaround_targets) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling $target with CMake $env:CONFIG"
    cmake --build "${binary_dir}" --config $env:CONFIG --target "${target}"
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling with CMake $env:CONFIG"
cmake --build "${binary_dir}" --config $env:CONFIG
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "cmake for 'all' target failed with exit code $LastExitCode"
    Exit ${LastExitCode}
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Running unit tests $env:CONFIG"
Set-Location "${binary_dir}"

if (Test-Path env:RUNNING_CI) {
    # On Kokoro we need to define %TEMP% or the tests do not have a valid directory for
    # temporary files.
    $env:TEMP="T:\tmp"
}

# Get the number of processors to parallelize the tests
$NCPU=(Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors

$ctest_args = @(
    "--output-on-failure",
    "-j", $NCPU,
    "-C", $env:CONFIG,
    "--progress"
)
# TODO(#15584): The ConnectionImplTest.MultiplexedPrecommitUpdated test
# is disabled.
$env:GTEST_FILTER = "-ConnectionImplTest.MultiplexedPrecommitUpdated"
Write-Host -ForegroundColor Yellow "Running ctest with GTEST_FILTER=${env:GTEST_FILTER}"
ctest $ctest_args -LE integration-test
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "ctest failed with exit code $LastExitCode"
    Exit ${LastExitCode}
}

# Import the functions and variables used to run integration tests
Set-Location "${project_root}"
. ci/kokoro/windows/lib/integration.ps1

if (Test-Integration-Enabled) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Running minimal quickstart programs $env:CONFIG"
    Install-Roots-Pem
    ${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}="${env:KOKORO_GFILE_DIR}/roots.pem"
    ${env:GOOGLE_APPLICATION_CREDENTIALS}="${env:KOKORO_GFILE_DIR}/kokoro-run-key.json"
    Set-Location "${binary_dir}"
    ctest $ctest_args -R "(storage_quickstart|pubsub_quickstart)"
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "ctest failed with exit code $LastExitCode"
        Exit ${LastExitCode}
    }
    Set-Location "${project_root}"
}

Write-Host -ForegroundColor Green "`n$(Get-Date -Format o) DONE"
