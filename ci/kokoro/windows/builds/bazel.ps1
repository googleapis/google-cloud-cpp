#!/usr/bin/env powershell
#
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
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

Fetch-Bazel-Dependencies

# All the build_flags should be set by now, so we'll copy them, and add a few
# more test-only flags.
$test_flags = $build_flags
$test_flags += @("--test_output=errors", "--verbose_failures=true")

Write-Host "`n$(Get-Date -Format o) Compiling and running unit tests"
# See #15678
$exclude_build_targets = @("-//google/cloud/bigtable:internal_query_plan_test", `
    "-//google/cloud/storage/tests:storage_include_test-default", `
    "-//google/cloud/storage/tests:storage_include_test-grpc-metadata", `
    "-//google/cloud/pubsub/samples:all")
bazelisk $common_flags test $test_flags --test_tag_filters=-integration-test ... -- $exclude_build_targets
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "bazel test failed with exit code ${LastExitCode}."
    Exit ${LastExitCode}
}

Write-Host "`n$(Get-Date -Format o) Compiling extra programs with bazel $common_flags build $build_flags ..."
bazelisk $common_flags build $build_flags ... -- $exclude_build_targets
if ($LastExitCode) {
    Write-Host -ForegroundColor Red "bazel build failed with exit code ${LastExitCode}."
    Exit ${LastExitCode}
}

# Import the functions and variables used to run integration tests
. ci/kokoro/windows/lib/integration.ps1

function Invoke-REST-Quickstart {
    param($bazel_bin)
    try {
        $executable = Join-Path $bazel_bin "google/cloud/storage/quickstart/quickstart.exe"
        Write-Host "Running REST Quickstart, attempting to run: $executable"
        if (-not (Test-Path $executable)) {
          Write-Host -ForegroundColor Red "Executable not found at the specified path."
          Exit 1
        }
        & $executable "${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
        if ($LastExitCode) {
            Write-Host -ForegroundColor Red "Execution of (storage/quickstart) failed with exit code ${LastExitCode}."
            Exit ${LastExitCode}
        }
    } catch {
        Write-Host -ForegroundColor Red "Caught exception while trying to run storage/quickstart: $_"
        Exit 1
    }
}

function Invoke-gRPC-Quickstart {
    param($bazel_bin)
    try {
        $executable = Join-Path $bazel_bin "google/cloud/pubsub/quickstart/quickstart.exe"
        Write-Host "Running gRPC Quickstart, attempting to run: $executable"
        if (-not (Test-Path $executable)) {
          Write-Host -ForegroundColor Red "Executable not found at the specified path."
          Exit 1
        }
        & $executable "${env:GOOGLE_CLOUD_PROJECT}" "${env:GOOGLE_CLOUD_CPP_PUBSUB_TEST_QUICKSTART_TOPIC}"
        if ($LastExitCode) {
            Write-Host -ForegroundColor Red "Execution of (pubsub/quickstart) failed with exit code ${LastExitCode}."
            Exit ${LastExitCode}
        }
    } catch {
        Write-Host -ForegroundColor Red "Caught exception while trying to run pubsub/quickstart: $_"
        Exit 1
    }
}

if (Test-Integration-Enabled) {
    Write-Host "`n$(Get-Date -Format o) Running minimal quickstart prorams"
    
    # 1. Install the certificates
    Install-Roots-Pem

    # 2. Normalize paths to use Forward Slashes (/)
    # This is critical for C++ binaries (BoringSSL/libcurl) to parse paths correctly on Windows.
    $RawRootsPath = Join-Path $env:KOKORO_GFILE_DIR "roots.pem"
    $RootsPath = $RawRootsPath -replace '\\', '/'
    
    $RawKeyPath = Join-Path $env:KOKORO_GFILE_DIR "kokoro-run-key.json"
    $KeyPath = $RawKeyPath -replace '\\', '/'

    # 3. Set ALL SSL Environment Variables
    # OpenSSL/BoringSSL may look at SSL_CERT_FILE before CURL_CA_BUNDLE
    $env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH = $RootsPath
    $env:CURL_CA_BUNDLE = $RootsPath
    $env:SSL_CERT_FILE = $RootsPath
    $env:GOOGLE_APPLICATION_CREDENTIALS = $KeyPath
    
    # 4. Enable Deep Library Logging
    $env:GOOGLE_CLOUD_CPP_ENABLE_TRACING="http"
    $env:CURL_VERBOSE="1"

    # --- DEBUG CHECKS ---
    Write-Host -ForegroundColor Cyan "`n--- DEBUG: Environment & File Check ---"
    Write-Host "Roots Path: $RootsPath"
    
    Write-Host "`n[Check 1] Environment Variables:"
    Get-ChildItem Env: | Where-Object { $_.Name -match 'CURL_|GOOGLE_|GRPC_|SSL_' } | Format-Table -AutoSize | Out-Host
    
    Write-Host "`n[Check 2] File Verify:"
    if (Test-Path $RootsPath) {
        Write-Host -ForegroundColor Green "File exists."
        Get-Item $RootsPath | Select-Object Length, LastWriteTime
    } else {
        Write-Host -ForegroundColor Red "CRITICAL: File not found at $RootsPath"
    }
    Write-Host "--- DEBUG END ---`n"

    bazelisk $common_flags build $build_flags `
        //google/cloud/storage/quickstart:quickstart `
        //google/cloud/pubsub/quickstart:quickstart

    $bazel_bin = (bazelisk $common_flags info $build_flags bazel-bin).Trim()
    # Fix bazel-bin path for PowerShell invocation just in case
    $bazel_bin = $bazel_bin.Replace('/', '\') 
    Write-Host "bazel-bin directory: $bazel_bin"

    # --- VERIFICATION EXPERIMENT START ---
    Write-Host -ForegroundColor Cyan "`n--- EXPERIMENT: The 'Strip & Retry' Test ---"
    
    # Define paths
    $DirtyFile = $RawRootsPath
    $CleanFile = Join-Path $env:KOKORO_GFILE_DIR "roots_clean.pem"
    $CleanFileForward = $CleanFile -replace '\\', '/'

    # Check for the "Poison" (\r)
    $text = [System.IO.File]::ReadAllText($DirtyFile)
    if ($text.Contains("`r")) {
        Write-Host -ForegroundColor Red "[CONFIRMED] 'roots.pem' contains Carriage Returns (\r)."
        Write-Host "      Attempting to sanitize and run binary..."
        
        # Create the Antidote (Remove all \r)
        $cleanText = $text.Replace("`r", "")
        [System.IO.File]::WriteAllText($CleanFile, $cleanText)
        Write-Host "Created sanitized file: $CleanFileForward"

        # Run the Binary against the CLEAN file
        Write-Host "`nRunning quickstart.exe using CLEAN file..."
        
        # Temporarily override the env var just for this test
        $env:CURL_CA_BUNDLE = $CleanFileForward
        $env:SSL_CERT_FILE = $CleanFileForward
        $env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH = $CleanFileForward
        
        # Construct executable path
        $QuickstartExe = Join-Path $bazel_bin "google/cloud/storage/quickstart/quickstart.exe"

        try {
           & $QuickstartExe "${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
           if ($LastExitCode -eq 0) {
               Write-Host -ForegroundColor Green "`n[SUCCESS] The binary worked with the clean file!"
               Write-Host -ForegroundColor Green "CONCLUSION: Carriage Returns were the root cause."
           } else {
               Write-Host -ForegroundColor Red "`n[FAILURE] The binary still failed ($LastExitCode) even with the clean file."
               Write-Host -ForegroundColor Red "CONCLUSION: The issue is NOT carriage returns."
           }
        } catch {
            Write-Host "Execution failed: $_"
        }
        
        # Restore Env Vars for standard test flow
        $env:CURL_CA_BUNDLE = $RootsPath
        $env:SSL_CERT_FILE = $RootsPath
        $env:GRPC_DEFAULT_SSL_ROOTS_FILE_PATH = $RootsPath
    } else {
        Write-Host -ForegroundColor Green "[INFO] 'roots.pem' is already clean (No \r). Experiment skipped."
    }
    Write-Host "------------------------------------------------"
    # --- VERIFICATION EXPERIMENT END ---

    Invoke-REST-Quickstart $bazel_bin
    Invoke-gRPC-Quickstart $bazel_bin
}

# Shutdown the Bazel server to release any locks
Write-Host "$(Get-Date -Format o) Shutting down Bazel server"
bazelisk $common_flags shutdown
bazelisk shutdown

Write-Host "`n$(Get-Date -Format o) DONE"
