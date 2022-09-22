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

. ci/kokoro/windows/lib/bazel.ps1

if ($args.count -ne 1) {
    Write-Host -ForegroundColor Red `
        "Aborting build, expected the build name as the first (and only) argument"
    Exit 1
}
$BuildName = $args[0]

$common_flags = Get-Bazel-Common-Flags

Write-Bazel-Config

$build_flags = Get-Bazel-Build-Flags

$project_root = (Get-Item -Path ".\" -Verbose).FullName

$libraries=("bigquery", "bigtable", "iam", "pubsub", "spanner", "storage")
$failures=()
ForEach($library in $libraries) {
    Set-Location "${project_root}/google/cloud/${library}/quickstart"
    Fetch-Bazel-Dependencies
    Write-Host -ForegroundColor Yellow "$(Get-Date -Format o) Build quickstart for ${library}"
    bazelisk $common_flags build $build_flags :quickstart
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "bazel build failed with exit code ${LastExitCode}."
        $failures += (${library})
    }
    # Shutdown the Bazel server to release any locks
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Shutting down Bazel server"
    bazelisk $common_flags shutdown
    bazelisk shutdown 
    Set-Location "${project_root}"
}

if ($failures.count -eq 0) {
    Write-Host -ForegroundColor Green "`n$(Get-Date -Format o) All quickstarts built successfully"
    Exit 0
}

Write-Host -ForegroundColor Red "`n$(Get-Date -Format o) The following quickstarts failed to build: ${failures}"
Exit 1
