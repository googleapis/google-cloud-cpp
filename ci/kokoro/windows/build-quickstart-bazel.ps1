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
bazel $common_flags version

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
    "Shutting down Bazel server"
bazel $common_flags shutdown

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
    $build_flags += @("--remote_cache=${BAZEL_CACHE}/windows/${BuildName}")
    $build_flags += @("--google_credentials=${env:KOKORO_GFILE_DIR}/kokoro-run-key.json")
    # See https://docs.bazel.build/versions/master/remote-caching.html#known-issues
    # and https://github.com/bazelbuild/bazel/issues/3360
    $build_flags += @("--experimental_guard_against_concurrent_changes")
}

$env:BAZEL_VC="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC"

$project_root = (Get-Item -Path ".\" -Verbose).FullName
# If at all possible, load the configuration for the integration tests and
# set ${env:RUN_INTEGRATION_TESTS} to "true"
if (-not (Test-Path env:KOKORO_GFILE_DIR)) {
    ${env:RUN_INTEGRATION_TESTS}=""
} else {
    $integration_tests_config="${project_root}/ci/etc/integration-tests-config.ps1"
    $test_key_file_json="${env:KOKORO_GFILE_DIR}/kokoro-run-key.json"
    $test_key_file_p12="${env:KOKORO_GFILE_DIR}/kokoro-run-key.p12"
    if ((Test-Path "${integration_tests_config}") -and
        (Test-Path "${test_key_file_json}") -and
        (Test-Path "${test_key_file_p12}")) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Loading integration tests config"
        . "${integration_tests_config}"
        ${env:GOOGLE_APPLICATION_CREDENTIALS}="${test_key_file_json}"
        ${env:GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES}="yes"
        (New-Object System.Net.WebClient).Downloadfile(
            'https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem',
             "${env:KOKORO_GFILE_DIR}/roots.pem")
        ${env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}="${env:KOKORO_GFILE_DIR}/roots.pem"
        ${env:RUN_INTEGRATION_TESTS}="true"
    }
}

$quickstart_args=@{
    "storage"=@("${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}");
    "bigtable"=@("${env:GOOGLE_CLOUD_PROJECT}", "${env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID}", "quickstart")
    "spanner"=@("${env:GOOGLE_CLOUD_PROJECT}", "${env:GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID}", "quickstart-db")
}

ForEach($library in ("bigtable", "storage", "spanner")) {
    Set-Location "${project_root}/google/cloud/${library}/quickstart"
    ForEach($_ in (1, 2, 3)) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Fetch dependencies for ${library} [$_]"
        bazel $common_flags fetch $build_flags ...
        if ($LastExitCode -eq 0) {
            break
        }
    }

    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
        "Compiling quickstart for ${library}"
    bazel $common_flags build $build_flags ...
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red `
            "bazel test failed with exit code ${LastExitCode}."
        Exit ${LastExitCode}
    }

    if ((Test-Path env:RUN_INTEGRATION_TESTS) -and ($env:RUN_INTEGRATION_TESTS -eq "true")) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Running quickstart for ${library}"
        bazel $common_flags run "--spawn_strategy=local" `
            ":quickstart" -- $quickstart_args[${library}]
        if ($LastExitCode) {
            Write-Host -ForegroundColor Red `
                "quickstart test for ${library} failed with exit code ${LastExitCode}."
            Exit ${LastExitCode}
        }
    }

    # Need to free up the open files or the caching below fails.
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
        "Shutting down Bazel for ${library}"
    bazel $common_flags shutdown
}
Set-Location "${project_root}"

# Shutdown the Bazel server to release any locks
Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Shutting down Bazel server"
bazel $common_flags shutdown
bazel shutdown

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) DONE"
