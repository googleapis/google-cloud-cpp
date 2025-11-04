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

function Debug-Network {
    param([string]$targetUrl)
    Write-Host -ForegroundColor Cyan "`n--- NETWORK DEBUG START ($targetUrl) ---"
    try {
        $uri = New-Object System.Uri($targetUrl)
        $hostName = $uri.DnsSafeHost

        # 1. DNS Resolution
        Write-Host "1. Testing DNS resolution for $hostName..."
        $dns = Resolve-DnsName -Name $hostName -ErrorAction SilentlyContinue
        if ($dns) { $dns | Format-Table -AutoSize | Out-Host } else { Write-Host -ForegroundColor Red "DNS Resolution FAILED" }

        # 2. Basic TCP Connectivity (checking port 443)
        Write-Host "`n2. Testing TCP connectivity to $hostName`:443..."
        try {
             $tcp = Test-NetConnection -ComputerName $hostName -Port 443 -WarningAction SilentlyContinue
             if ($tcp.TcpTestSucceeded) { Write-Host "TCP connection SUCCEEDED" } else { Write-Host -ForegroundColor Red "TCP connection FAILED" }
             Write-Host "Detailed Info: $($tcp | Out-String)"
        } catch {
             Write-Host -ForegroundColor Red "Test-NetConnection failed to run: $_"
        }

        # 3. Proxy Detection
        Write-Host "`n3. Checking System Proxy for $targetUrl..."
        $proxy = [System.Net.WebRequest]::GetSystemWebProxy()
        $proxyUri = $proxy.GetProxy($uri)
        Write-Host "Effective Proxy: $proxyUri"
        Write-Host "Is Bypassed: $($proxy.IsBypassed($uri))"

    } catch {
        Write-Host -ForegroundColor Red "An error occurred during network debug: $_"
    }
    Write-Host -ForegroundColor Cyan "--- NETWORK DEBUG END ---`n"
}

function Install-Roots-Pem {
    Debug-Network -targetUrl "https://curl.se/ca/cacert.pem"

    ForEach($attempt in (1, 2, 3)) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Downloading roots.pem [$attempt]"
        try {
            (New-Object System.Net.WebClient).Downloadfile(
                    'https://curl.se/ca/cacert.pem',
                    "${env:KOKORO_GFILE_DIR}/roots.pem")

            Write-Host "Appending Windows System Root Certificates to roots.pem..."
            $certStore = New-Object System.Security.Cryptography.X509Certificates.X509Store -ArgumentList "Root", "LocalMachine"
            $certStore.Open('ReadOnly')
            $certStore.Certificates | ForEach-Object {
                $cert = $_
                # Prepend distinct newlines to ensure separation from the previous file content
                $pem = "`r`n-----BEGIN CERTIFICATE-----`r`n" +
                       [Convert]::ToBase64String($cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert), 'InsertLineBreaks') +
                       "`r`n-----END CERTIFICATE-----`r`n"
                Add-Content -Path "${env:KOKORO_GFILE_DIR}/roots.pem" -Value $pem -Encoding Ascii
            }
            $certStore.Close()

            # --- DEBUG START ---
            $pemPath = "${env:KOKORO_GFILE_DIR}/roots.pem"
            Write-Host -ForegroundColor Cyan "`nDEBUG: Inspecting roots.pem..."

            Write-Host "--- HEAD (First 10 lines) ---"
            Get-Content $pemPath -TotalCount 10

            Write-Host "`n--- TAIL (Last 20 lines) ---"
            Get-Content $pemPath -Tail 20

            # CRITICAL CHECK: Search for missing newlines at the join point
            Write-Host "`n--- SEAM CHECK ---"
            $corruption = Select-String -Path $pemPath -Pattern "-----END CERTIFICATE----------BEGIN CERTIFICATE-----"
            if ($corruption) {
                Write-Host -ForegroundColor Red "FAIL: Found corrupted certificate boundaries (missing newline)!"
            } else {
                Write-Host -ForegroundColor Green "PASS: No certificate boundary corruption detected."
            }

            Write-Host -ForegroundColor Cyan "`nDEBUG: Testing SSL connection to GCS..."
            try {
                & curl.exe --version
                & curl.exe -v https://storage.googleapis.com --cacert $pemPath 2>&1 | Out-Host
                if ($LastExitCode -ne 0) {
                    Write-Host -ForegroundColor Red "Curl exited with error code: $LastExitCode"
                }
            } catch {
                 Write-Host -ForegroundColor Red "Debug curl command failed: $_"
            }
            # --- DEBUG END ---

            return
        } catch {
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) download/setup error: $_"
            if ($attempt -eq 3) {
                 Debug-Network -targetUrl "https://storage.googleapis.com"
            }
        }
        Start-Sleep -Seconds (60 * $attempt)
    }
    Write-Host -ForegroundColor Red "cannot download roots.pem file."
    Exit 1
}
