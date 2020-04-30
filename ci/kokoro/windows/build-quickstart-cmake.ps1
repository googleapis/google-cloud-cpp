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

# First check the required environment variables.
$missing=@()
ForEach($var in ("CONFIG", "GENERATOR", "VCPKG_TRIPLET")) {
    if (-not (Test-Path env:${var})) {
        $missing+=(${var})
    }
}
if ($missing.count -ge 1) {
    throw "Aborting build because the ${missing} environment variables are not set."
}

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
}

ForEach($library in ("bigtable", "storage")) {
    Set-Location "${project_root}"

    $binary_dir="cmake-out/quickstart-${library}-${env:VCPKG_TRIPLET}"
    # By default assume "module", use the configuration parameters and build in the `cmake-out` directory.
    $cmake_flags=@("-G${env:GENERATOR}", "-DCMAKE_BUILD_TYPE=$env:CONFIG", "-Hgoogle/cloud/${library}/quickstart", "-B${binary_dir}")
    # Setup the environment for vcpkg:
    $cmake_flags += "-DCMAKE_TOOLCHAIN_FILE=`"${project_root}\cmake-out\vcpkg-quickstart\scripts\buildsystems\vcpkg.cmake`""
    $cmake_flags += "-DVCPKG_TARGET_TRIPLET=$env:VCPKG_TRIPLET"
    $cmake_flags += "-DCMAKE_C_COMPILER=cl.exe"
    $cmake_flags += "-DCMAKE_CXX_COMPILER=cl.exe"
    $cmake_flags += "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"

    # Configure CMake and create the build directory.
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Configuring CMake with $cmake_flags"
    cmake $cmake_flags
    if ($LastExitCode) {
        throw "cmake config failed with exit code $LastExitCode"
    }

    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Compiling with CMake $env:CONFIG"
    cmake --build "${binary_dir}" --config $env:CONFIG
    if ($LastExitCode) {
        throw "cmake for 'all' target failed with exit code $LastExitCode"
    }

    if ((Test-Path env:RUN_INTEGRATION_TESTS) -and ($env:RUN_INTEGRATION_TESTS -eq "true")) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Running quickstart for ${library}"
        Set-Location "${binary_dir}"
        .\quickstart.exe $quickstart_args[${library}]
        if ($LastExitCode) {
            throw "quickstart test for ${library} failed with exit code ${LastExitCode}."
        }
    }
}

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) DONE"
