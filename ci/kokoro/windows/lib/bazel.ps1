# Copyright 2022 Google LLC
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

# Helper functions to configure Bazel

$Env:USE_BAZEL_VERSION="5.4.0"

# Create output directory for Bazel. Bazel creates really long paths,
# sometimes exceeding the Windows limits. Using a short name for the
# root of the Bazel output directory works around this problem.
$bazel_root="C:\b"
if (Test-Path "T:\") {
  # Prefer the tmpfs volume on Kokoro builds. It is supposed to be faster, and
  # it is sized appropriately (250G as I write this)
  $bazel_root="T:\b"
}
if (-not (Test-Path $bazel_root)) {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Create bazel user root (${bazel_root})"
    New-Item -ItemType Directory -Path $bazel_root | Out-Null
}

function Get-Bazel-Common-Flags {
    return @("--output_user_root=${bazel_root}")
}

function Get-Bazel-Build-Flags {
    param ($BuildName)
    # These flags are shared by all builds
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
    if ($BuildName -like "bazel-release*") {
        $build_flags += ("-c", "opt")
    }

    if (-not (Test-Path env:KOKORO_GFILE_DIR)) {
        return $build_flags
    }
    if (-not (Test-Path "${env:KOKORO_GFILE_DIR}/kokoro-run-key.json")) {
        return $build_flags
    }

    # If we have the right credentials, tell bazel to cache build results in a GCS
    # bucket. Note: this will not cache external deps, so the "fetch" below will
    # not hit this cache.
    Write-Host "$(Get-Date -Format o) Using bazel remote cache: ${BAZEL_CACHE}/windows/${BuildName}"
    $BAZEL_CACHE="https://storage.googleapis.com/cloud-cpp-bazel-cache"
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
        "--remote_timeout=5",
        "--google_credentials=${env:KOKORO_GFILE_DIR}/kokoro-run-key.json",
        # See https://docs.bazel.build/versions/main/remote-caching.html#known-issues
        # and https://github.com/bazelbuild/bazel/issues/3360
        "--experimental_guard_against_concurrent_changes"
    )

    return $build_flags
}

function Write-Bazel-Config {
    $common_flags = Get-Bazel-Common-Flags
    Write-Host "`n$(Get-Date -Format o) Capture Bazel information for troubleshooting"
    bazelisk $common_flags version

    # Shutdown the Bazel server to release any locks
    Write-Host "$(Get-Date -Format o) Shutting down Bazel server"
    bazelisk $common_flags shutdown
}

function Fetch-Bazel-Dependencies {
    $common_flags = Get-Bazel-Common-Flags
    # Additional dependencies, these are not downloaded by `bazel fetch ...`,
    # but are needed to compile the code
    $external=@(
        "@local_config_platform//...",
        "@local_config_cc_toolchains//...",
        "@local_config_sh//...",
        "@go_sdk//...",
        "@remotejdk11_win//:jdk"
    )
    ForEach($_ in (1, 2)) {
        Write-Host "$(Get-Date -Format o) Fetch dependencies [$_]"
        bazelisk $common_flags fetch --noshow_loading_progress ${external} ...
        if ($LastExitCode -eq 0) {
            return
        }
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Fetch attempt $_ failed, trying again"
        Start-Sleep -Seconds (60 * $_)
    }
    Write-Host "$(Get-Date -Format o) Fetch dependencies (last attempt)"
    bazelisk $common_flags fetch --noshow_loading_progress ${external} ...
}
