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
    $RootsPath = "${env:KOKORO_GFILE_DIR}/roots.pem"

    ForEach($attempt in (1, 2, 3)) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Downloading roots.pem [$attempt]"
        try {
            # 1. Download the Mozilla Bundle to memory string
            # We avoid saving to disk immediately to prevent PowerShell from adding CRLF
            $WebClient = New-Object System.Net.WebClient
            $MozillaCerts = $WebClient.DownloadString('https://curl.se/ca/cacert.pem')

            # 2. Gather Windows System Certificates
            # We check both 'Root' (Trusted Root CAs) and 'CA' (Intermediate CAs)
            # as corporate proxies often sign with an Intermediate.
            Write-Host "Gathering Windows System Root Certificates..."
            $WindowsCerts = ""
            $storesToCheck = @("Root", "CA")
            
            foreach ($storeName in $storesToCheck) {
                Write-Host -ForegroundColor Cyan "Processing Store: LocalMachine\$storeName"
                $certStore = New-Object System.Security.Cryptography.X509Certificates.X509Store -ArgumentList $storeName, "LocalMachine"
                $certStore.Open('ReadOnly')
                
                $certStore.Certificates | ForEach-Object {
                    $cert = $_
                    Write-Host "  Adding: $($cert.Subject)"
                    
                    # Export to Base64
                    $b64 = [Convert]::ToBase64String($cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert), 'InsertLineBreaks')
                    
                    # Construct PEM with explicit Unix Newlines (\n)
                    $header = "-----BEGIN CERTIFICATE-----"
                    $footer = "-----END CERTIFICATE-----"
                    $WindowsCerts += "$header`n$b64`n$footer`n"
                }
                $certStore.Close()
            }

            # 3. Write Combined File with Strict UNIX Line Endings (\n)
            # We use .NET IO classes to bypass PowerShell's default CRLF behavior.
            Write-Host "Writing combined roots.pem with Unix LF line endings..."
            $FinalContent = $MozillaCerts + "`n" + $WindowsCerts
            
            # Normalize: Replace any Windows \r\n with Unix \n
            # This is the CRITICAL FIX for BoringSSL/gRPC which can choke on Carriage Returns (\r)
            $FinalContent = $FinalContent -replace "`r`n", "`n"
            
            [System.IO.File]::WriteAllText($RootsPath, $FinalContent, [System.Text.Encoding]::ASCII)

            # --- DEBUG START ---
            Write-Host -ForegroundColor Cyan "`nDEBUG: Inspecting roots.pem..."
            
            # Check for Seams/Corruption
            $corruption = Select-String -Path $RootsPath -Pattern "-----END CERTIFICATE----------BEGIN CERTIFICATE-----"
            if ($corruption) {
                Write-Host -ForegroundColor Red "FAIL: Found corrupted certificate boundaries!"
            } else {
                Write-Host -ForegroundColor Green "PASS: No certificate boundary corruption detected."
            }

            # Check for Carriage Returns (The "BoringSSL Killer")
            if ($FinalContent.Contains("`r")) {
                 Write-Host -ForegroundColor Red "FAIL: File still contains Carriage Returns (\r)!"
            } else {
                 Write-Host -ForegroundColor Green "PASS: File contains strict Unix Line Feeds (\n)."
            }

            Write-Host -ForegroundColor Cyan "`nDEBUG: Testing SSL connection to GCS..."
            
            # Relax ErrorActionPreference so curl -v stderr doesn't crash the script
            $OldEAP = $ErrorActionPreference
            $ErrorActionPreference = "Continue"
            
            try {
                & curl.exe --version
                & curl.exe -v https://storage.googleapis.com --cacert $RootsPath 2>&1 | Out-Host
                if ($LastExitCode -ne 0) {
                    Write-Host -ForegroundColor Red "Curl exited with error code: $LastExitCode"
                } else {
                     Write-Host -ForegroundColor Green "Curl connection test PASSED."
                }
            } catch {
                 Write-Host -ForegroundColor Red "Debug curl command failed unexpectedly: $_"
            } finally {
                $ErrorActionPreference = $OldEAP
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
