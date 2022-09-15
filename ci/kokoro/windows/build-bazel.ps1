#!/usr/bin/env powershell
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
# We do this in C: (the boot disk) because T: (the tmpfs / build disk)
# is too small for the Bazel directory.
$bazel_root="C:\b"
if (-not (Test-Path $bazel_root)) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Create bazel user root (${bazel_root})"
    New-Item -ItemType Directory -Path $bazel_root | Out-Null
}

$common_flags = @("--output_user_root=${bazel_root}")

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Capture Bazel information for troubleshooting"
$Env:USE_BAZEL_VERSION="5.3.0"
bazelisk $common_flags version

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Shutting down Bazel server"
bazelisk $common_flags shutdown

$download_dir = "T:\tmp"
if (Test-Path env:TEMP) {
    $download_dir="${env:TEMP}"
} elseif (-not (Test-Path "${download_dir}")) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Create download directory ${download_dir}"
    Make-Item -Type "Directory" ${download_dir}
}
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Using ${download_dir} as download directory"

$build_flags = @(
    "--keep_going",
    "--per_file_copt=^//google/cloud@-W3",
    "--per_file_copt=^//google/cloud@-WX",
    "--per_file_copt=^//google/cloud@-experimental:external",
    "--per_file_copt=^//google/cloud@-external:W0",
    "--per_file_copt=^//google/cloud@-external:anglebrackets",
    # Disable warnings on generated proto files.
    "--per_file_copt=.*\.pb\.cc@/wd4244"
)

$BAZEL_CACHE="https://storage.googleapis.com/cloud-cpp-bazel-cache"

# If we have the right credentials, tell bazel to cache build results in a GCS
# bucket. Note: this will not cache external deps, so the "fetch" below will
# not hit this cache.
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

# TODO(#9531) - enable release builds for MSVC 2017
if (($BuildName -like "bazel-release*") -and ($BuildName -ne "bazel-release-2017")) {
    $build_flags += ("-c", "opt")
}

# Before MSVC 2022 the compiler is a 32-bit binary
if ("${env:MSVC_VERSION}" -ge "2022") {
  $env:BAZEL_VC="C:\Program Files\Microsoft Visual Studio\${env:MSVC_VERSION}\Community\VC"
} else {
  $env:BAZEL_VC="C:\Program Files (x86)\Microsoft Visual Studio\${env:MSVC_VERSION}\Community\VC"
}

ForEach($_ in (1, 2, 3)) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Fetch dependencies [$_]"
    # Additional dependencies, these are not downloaded by `bazel fetch ...`,
    # but are needed to compile the code
    $external=@(
        "@local_config_platform//...",
        "@local_config_cc_toolchains//...",
        "@local_config_sh//...",
        "@go_sdk//...",
        "@remotejdk11_win//:jdk"
    )
    bazelisk $common_flags fetch ${external} ...
    if ($LastExitCode -eq 0) {
        break
    }
    Start-Sleep -Seconds (60 * $_)
}

# All the build_flags should be set by now, so we'll copy them, and add a few
# more test-only flags.
$test_flags = $build_flags
$test_flags += @("--test_output=errors", "--verbose_failures=true")

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling and running unit tests"
bazelisk $common_flags test $test_flags --test_tag_filters=-integration-test ...
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "bazel test failed with exit code ${LastExitCode}."
    Exit ${LastExitCode}
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling extra programs"
Write-Host -ForegroundColor Yellow bazel $common_flags build $build_flags ...
bazelisk $common_flags build $build_flags ...
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "bazel build failed with exit code ${LastExitCode}."
    Exit ${LastExitCode}
}

# Import the functions and variables used to run integration tests
. ci/kokoro/windows/lib/integration.ps1

function Invoke-REST-Quickstart {
    bazelisk $common_flags run $build_flags `
      //google/cloud/storage/quickstart:quickstart -- `
      "${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "bazel run (storage/quickstart) failed with exit code ${LastExitCode}."
        Exit ${LastExitCode}
    }
}

function Invoke-gRPC-Quickstart {
    bazelisk $common_flags run $build_flags `
      //google/cloud/pubsub/quickstart:quickstart -- `
      "${env:GOOGLE_CLOUD_PROJECT}" "${env:GOOGLE_CLOUD_CPP_PUBSUB_TEST_QUICKSTART_TOPIC}"
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "bazel run (pubsub/quickstart) failed with exit code ${LastExitCode}."
        Exit ${LastExitCode}
    }
}

if (Test-Integration-Enabled) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Running minimal quickstart prorams"
    Install-Roots-Pem
    ${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}="${env:KOKORO_GFILE_DIR}/roots.pem"
    ${env:GOOGLE_APPLICATION_CREDENTIALS}="${env:KOKORO_GFILE_DIR}/kokoro-run-key.json"
    Invoke-REST-Quickstart
    Invoke-gRPC-Quickstart
}

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Shutting down Bazel server"
bazelisk $common_flags shutdown
bazelisk shutdown

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) DONE"
