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

# If possible, configure the vcpkg cache.
. ci/kokoro/windows/lib/vcpkg-cache.ps1

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

Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) DONE"
