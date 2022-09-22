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
} else {
    Write-Host -ForegroundColor Red `
        "Aborting build, expected the build name as the first (and only) argument"
    Exit 1
}

# Create output directory for Bazel. Bazel creates really long paths,
# sometimes exceeding the Windows limits. Using a short name for the
# root of the Bazel output directory works around this problem.
$bazel_root="C:\b"
if (-not (Test-Path $bazel_root)) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Create bazel user root (${bazel_root})"
    New-Item -ItemType Directory -Path $bazel_root | Out-Null
}

$common_flags = @("--output_user_root=${bazel_root}")

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Capture Bazel information for troubleshooting"
$Env:USE_BAZEL_VERSION="5.3.1"
bazelisk $common_flags version

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
    "Shutting down Bazel server"
bazelisk $common_flags shutdown

$download_dir = "T:\tmp"
if (Test-Path env:TEMP) {
    $download_dir="${env:TEMP}"
} elseif (-not $download_dir) {
    Make-Item -Type "Directory" ${download_dir}
}

$BAZEL_CACHE="https://storage.googleapis.com/cloud-cpp-bazel-cache"

# If we have the right credentials, tell bazel to cache build results in a GCS
# bucket. Note: this will not cache external deps, so the "fetch" below will
# not hit this cache.
$build_flags=@()
if ((Test-Path env:KOKORO_GFILE_DIR) -and
    (Test-Path "${env:KOKORO_GFILE_DIR}/kokoro-run-key.json")) {
    Write-Host -ForegroundColor Yellow "Using bazel remote cache: ${BAZEL_CACHE}/windows/${BuildName}"
    $build_flags += @(
        "--remote_cache=${BAZEL_CACHE}/windows/${BuildName}",
        # Reduce the timeout for the remote cache from the 60s default:
        #     https://docs.bazel.build/versions/main/command-line-reference.html#flag--remote_timeout
        # If the build machine has network problems we would rather build
        # locally over blocking the build for 60s. When adjusting this
        # parameter, keep in mind that:
        # - Some of the objects in the cache in the ~60MiB range.
        # - Without tuning uploads run in the 50 MiB/s range, and downloads in
        #   the 150 MiB/s range.
        "--remote_timeout=5"
        )
    $build_flags += @("--google_credentials=${env:KOKORO_GFILE_DIR}/kokoro-run-key.json")
    # See https://docs.bazel.build/versions/main/remote-caching.html#known-issues
    # and https://github.com/bazelbuild/bazel/issues/3360
    $build_flags += @("--experimental_guard_against_concurrent_changes")
}

$project_root = (Get-Item -Path ".\" -Verbose).FullName
# If at all possible, load the configuration for the integration tests and
# set ${env:RUN_INTEGRATION_TESTS} to "true"
if (-not (Test-Path env:KOKORO_GFILE_DIR)) {
    ${env:RUN_INTEGRATION_TESTS}=""
} else {
    $integration_tests_config="${project_root}/ci/etc/integration-tests-config.ps1"
    $test_key_file_json="${env:KOKORO_GFILE_DIR}/kokoro-run-key.json"
    if ((Test-Path "${integration_tests_config}") -and
        (Test-Path "${test_key_file_json}")) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Loading integration tests config"
        . "${integration_tests_config}"
        ${env:GOOGLE_APPLICATION_CREDENTIALS}="${test_key_file_json}"
        ${env:GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES}="yes"
        ForEach($_ in (1, 2, 3)) {
            if ( $_ -ne 1) {
                Start-Sleep -Seconds (60 * $_)
            }
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Downloading roots.pem [$_]"
            try {
                (New-Object System.Net.WebClient).Downloadfile(
                        'https://pki.google.com/roots.pem',
                        "${env:KOKORO_GFILE_DIR}/roots.pem")
                break
            } catch {
                Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) download error"
            }
        }
        ${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}="${env:KOKORO_GFILE_DIR}/roots.pem"
        ${env:RUN_INTEGRATION_TESTS}="true"
    }
}

Set-Location "${project_root}"

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Shutting down Bazel server"
bazelisk $common_flags shutdown
bazelisk shutdown

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) DONE"
