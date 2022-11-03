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

. ci/kokoro/windows/lib/bazel.ps1

$common_flags = Get-Bazel-Common-Flags

Write-Bazel-Config

$build_flags = Get-Bazel-Build-Flags "${BuildName}"

$project_root = (Get-Item -Path ".\" -Verbose).FullName

# Get the quickstart programs available in the previous `google-cloud-cpp` version.
function Get-Released-Quickstarts {
    param([string]$project_root, [string[]]$bazel_common_flags)

    Push-Location "${project_root}/google/cloud/bigtable/quickstart"
    bazelisk $bazel_common_flags version | Out-Null
    bazelisk $bazel_common_flags query --noshow_progress --noshow_loading_progress "filter(/quickstart:quickstart$, kind(cc_binary, @com_github_googleapis_google_cloud_cpp//google/...))" |
        ForEach-Object { $_.replace("@com_github_googleapis_google_cloud_cpp//google/cloud/", "").replace("/quickstart:quickstart", "") } |
        # The following quickstarts have problems building on Windows:
        #   TODO(#8145) - asset (TRUE/FALSE macros)
        #   TODO(#9340) - beyondcorp (long pathnames)
        #   TODO(#8125) - channel (DOMAIN macro)
        #   TODO(#8785) - storagetransfer (UID_MAX/GID_MAX macros)
        Where-Object { -not ("asset", "beyondcorp", "channel", "storagetransfer" -contains $_) } |
        # TODO(#9923) - compiling all quickstarts on Windows is too slow
        Get-Random -Count 10
    Pop-Location
}

$libraries = Get-Released-Quickstarts $project_root $common_flags
Write-Host "`n$(Get-Date -Format o) Building the following subset of the quickstarts: [${libraries}]"
$failures=@()
ForEach($library in $libraries) {
    Write-Host "`n$(Get-Date -Format o) Build quickstart for ${library}"
    Set-Location "${project_root}/google/cloud/${library}/quickstart"
    Fetch-Bazel-Dependencies
    bazelisk $common_flags build $build_flags :quickstart
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "bazel build failed with exit code ${LastExitCode}."
        $failures += (${library})
    }
    # Shutdown the Bazel server to release any locks
    Write-Host "$(Get-Date -Format o) Shutting down Bazel server"
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
