# Copyright 2022 Google LLC
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

# Helper functions to run the minimal integration tests

$PROJECT_ROOT = (Get-Item -Path ".\").FullName
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
    $RootsPath = "${env:KOKORO_GFILE_DIR}/roots.pem"

    ForEach($attempt in (1, 2, 3)) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Downloading roots.pem [$attempt]"
        try {
            # 1. Download the Mozilla Bundle
            $WebClient = New-Object System.Net.WebClient
            $MozillaCerts = $WebClient.DownloadString('https://curl.se/ca/cacert.pem')

            # 2. Gather Windows System Root Certificates
            # This is required because some corporate/cloud environments inspect traffic
            # using an intermediate CA present in the system store but not in Mozilla's bundle.
            Write-Host "Gathering Windows System Root Certificates..."
            $WindowsCerts = ""
            $storesToCheck = @("Root", "CA")
            
            foreach ($storeName in $storesToCheck) {
                $certStore = New-Object System.Security.Cryptography.X509Certificates.X509Store -ArgumentList $storeName, "LocalMachine"
                $certStore.Open('ReadOnly')
                
                $certStore.Certificates | ForEach-Object {
                    $cert = $_
                    $b64 = [Convert]::ToBase64String($cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert), 'InsertLineBreaks')
                    $header = "-----BEGIN CERTIFICATE-----"
                    $footer = "-----END CERTIFICATE-----"
                    $WindowsCerts += "$header`n$b64`n$footer`n"
                }
                $certStore.Close()
            }

            # 3. Write Combined File with strict Unix Line Endings (\n)
            # BoringSSL/gRPC can sometimes have issues with Windows CRLF.
            Write-Host "Writing combined roots.pem with Unix LF line endings..."
            $FinalContent = $MozillaCerts + "`n" + $WindowsCerts
            $FinalContent = $FinalContent -replace "`r`n", "`n"
            
            [System.IO.File]::WriteAllText($RootsPath, $FinalContent, [System.Text.Encoding]::ASCII)
            
            return
        } catch {
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) download/setup error: $_"
        }
        Start-Sleep -Seconds (60 * $attempt)
    }
    Write-Host -ForegroundColor Red "cannot setup roots.pem file."
    Exit 1
}
