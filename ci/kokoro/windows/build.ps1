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

$BuildName = ""
if ($args.count -eq 1) {
    $BuildName = $args[0]
} elseif (Test-Path env:KOKORO_JOB_NAME) {
    # Kokoro injects the KOKORO_JOB_NAME environment variable, the value of this
    # variable is cloud-cpp/common/<config-file-name-without-cfg> (or more
    # generally <path/to/config-file-without-cfg>). By convention we name these
    # files `$foo.cfg` for continuous builds and `$foo-presubmit.cfg` for
    # presubmit builds. Here we extract the value of "foo" and use it as the build
    # name.
    $BuildName = Split-Path -Path $env:KOKORO_JOB_NAME -Leaf
    $BuildName = $BuildName -replace "-presubmit",""

    # This is passed into the environment of the docker build and its scripts to
    # tell them if they are running as part of a CI build rather than just a
    # human invocation of "build.sh <build-name>". This allows scripts to be
    # strict when run in a CI, but a little more friendly when run by a human.
    $env:RUNNING_CI = "yes"
} else {
    Write-Host -ForegroundColor Red @"
Aborting build as the build name is not defined.
If you are invoking this script via the command line use:

$PSCommandPath <build-name>

If this script is invoked by Kokoro, the CI system is expected to set
the KOKORO_JOB_NAME environment variable.
"@
    Exit 1
}

if ($BuildName -eq "cmake-debug") {
    $env:CONFIG = "Debug"
    $env:GENERATOR = "Ninja"
    $env:VCPKG_TRIPLET = "x64-windows-static"
    $BuildScript = "builds/cmake.ps1"
} elseif ($BuildName -eq "cmake-release") {
    $env:CONFIG = "Release"
    $env:GENERATOR = "Ninja"
    $env:VCPKG_TRIPLET = "x64-windows-static"
    $BuildScript = "builds/cmake.ps1"
} elseif ($BuildName -eq "cmake-debug-x86") {
    $env:CONFIG = "Debug"
    $env:GENERATOR = "Ninja"
    $env:VCPKG_TRIPLET = "x86-windows-static"
    $BuildScript = "builds/cmake.ps1"
} elseif ($BuildName -eq "cmake-release-x86") {
    $env:CONFIG = "Release"
    $env:GENERATOR = "Ninja"
    $env:VCPKG_TRIPLET = "x86-windows-static"
    $BuildScript = "builds/cmake.ps1"
} elseif ($BuildName -like "bazel*") {
    $BuildScript = "builds/bazel.ps1"
} elseif ($BuildName -eq "quickstart-bazel") {
    $BuildScript = "builds/quickstart-bazel.ps1"
} elseif ($BuildName -eq "quickstart-cmake-static") {
    $env:CONFIG = "Debug"
    $env:GENERATOR = "Ninja"
    $env:VCPKG_TRIPLET = "x64-windows-static"
    $BuildScript = "builds/quickstart-cmake.ps1"
} elseif ($BuildName -eq "quickstart-cmake-dll") {
    $env:CONFIG = "Debug"
    $env:GENERATOR = "Ninja"
    $env:VCPKG_TRIPLET = "x64-windows"
    $BuildScript = "builds/quickstart-cmake.ps1"
}

$ScriptLocation = Split-Path $PSCommandPath -Parent

Write-Host -ForegroundColor Green "`n$(Get-Date -Format o) Running build script for $BuildName build"
powershell -exec bypass "${ScriptLocation}/${BuildScript}" "${BuildName}"
# Save the build exit code, we want to delete the artifacts
# even if the build fails.
$BuildExitCode = $LastExitCode

# Remove most things from the artifacts directory. Kokoro copies these files
# *very* slowly on Windows, and then ignores most of them :shrug:
if (Test-Path env:KOKORO_ARTIFACTS_DIR) {
    try {
        $ErrorActionPreference = "SilentlyContinue"
        Get-ChildItem -Recurse -File `
            -Exclude test.xml,sponge_log.xml,build.bat,build-32.bat,build.ps1 `
            -ErrorAction SilentlyContinue `
            -Path "${env:KOKORO_ARTIFACTS_DIR}" | `
            Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    } catch {
        Write-Host "$(Get-Date -Format o) error cleaning up KOKORO_ARTIFACTS_DIR, ignored"
    }
}

if ($BuildExitCode) {
    Write-Host -ForegroundColor Red "Build failed with exit code ${BuildExitCode}"
    Exit ${BuildExitCode}
}

Write-Host -ForegroundColor Green "Build completed successfully"
Exit 0
