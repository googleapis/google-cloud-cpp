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
    # Run pre-flight network checks to aid debugging if downloading fails later.
    Debug-Network -targetUrl "https://curl.se/ca/cacert.pem"

    ForEach($attempt in (1, 2, 3)) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Downloading roots.pem [$attempt]"
        try {
            # Use the standard Mozilla CA bundle from curl.se.
            # This is more comprehensive than pki.google.com and includes
            # the DigiCert roots currently used by some GCS endpoints.
            (New-Object System.Net.WebClient).Downloadfile(
                    'https://curl.se/ca/cacert.pem',
                    "${env:KOKORO_GFILE_DIR}/roots.pem")

	    # Append Windows System Trusted Roots to the downloaded bundle
            # This is required if the environment uses an SSL-inspecting proxy with a private CA.
            Write-Host "Appending Windows System Root Certificates to roots.pem..."
            $certStore = New-Object System.Security.Cryptography.X509Certificates.X509Store -ArgumentList "Root", "LocalMachine"
            $certStore.Open('ReadOnly')
            $certStore.Certificates | ForEach-Object {
                $cert = $_
                $pem = "-----BEGIN CERTIFICATE-----`r`n" +
                       [Convert]::ToBase64String($cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert), 'InsertLineBreaks') +
                       "`r`n-----END CERTIFICATE-----`r`n"
                Add-Content -Path "${env:KOKORO_GFILE_DIR}/roots.pem" -Value $pem
            }
            $certStore.Close()

            # --- DEBUG START ---
            # Verify the downloaded file works for GCS connections
            Write-Host -ForegroundColor Cyan "`nDEBUG: Testing SSL connection to GCS with verbose output..."
            try {
                # We use curl.exe (if available) to debug the handshake using the same CA bundle.
                # standard error (2) is redirected to standard output (1) so we see the verbose logs.
                & curl.exe -v https://storage.googleapis.com --cacert "${env:KOKORO_GFILE_DIR}/roots.pem" 2>&1 | Out-Host
            } catch {
                 Write-Host -ForegroundColor Red "Debug curl command failed to run."
            }
            # --- DEBUG END ---

            return
        } catch {
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) download error: $_"
            # If download fails, maybe the proxy blocks curl.se but allows google.com?
            # Let's debug the GCS endpoint too if we are failing.
            if ($attempt -eq 3) {
                 Debug-Network -targetUrl "https://storage.googleapis.com"
            }
        }
        Start-Sleep -Seconds (60 * $attempt)
    }
    Write-Host -ForegroundColor Red "cannot download roots.pem file."
    Exit 1
}
