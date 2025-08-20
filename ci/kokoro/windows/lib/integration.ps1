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

# Helper functions to run the minimal integration tests

$PROJECT_ROOT = (Get-Item -Path ".\" -Verbose).FullName
$integration_tests_config="${PROJECT_ROOT}/ci/etc/integration-tests-config.ps1"
. "${integration_tests_config}"

function Test-Integration-Enabled {
    if ((Test-Path env:KOKORO_GFILE_DIR) -and
       (Test-Path "${env:KOKORO_GFILE_DIR}/kokoro-run-key.json")) {
        return $True
    }
    return $False
}

function Install-Roots-Pem {
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
        "Downloading roots.pem [$_]"
    ForEach($attempt in (1, 2, 3)) {
        try {
            (New-Object System.Net.WebClient).Downloadfile(
                    'https://pki.google.com/roots.pem',
                    "${env:KOKORO_GFILE_DIR}/roots.pem")
            return
        } catch {
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) download error"
        }
        Start-Sleep -Seconds (60 * $attempt)
    }
    Write-Host -ForegroundColor Red "cannot download roots.pem file."
    Exit 1
}
