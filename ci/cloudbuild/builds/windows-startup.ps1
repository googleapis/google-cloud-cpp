# Setup log output redirection
$LogPath = "C:\build.log"
Start-Transcript -Path $LogPath

try {
    # 1. Fetch metadata values
    $MetadataUrl = "http://metadata.google.internal/computeMetadata/v1/instance/attributes"
    $Headers = @{"Metadata-Flavor"="Google"}

    $SourceArchive = Invoke-RestMethod -Headers $Headers -Uri "$MetadataUrl/source-archive"
    $BuildType = Invoke-RestMethod -Headers $Headers -Uri "$MetadataUrl/build-type"
    $Features = Invoke-RestMethod -Headers $Headers -Uri "$MetadataUrl/features"
    $LogsBucket = Invoke-RestMethod -Headers $Headers -Uri "$MetadataUrl/logs-bucket"
    $VcpkgVersion = Invoke-RestMethod -Headers $Headers -Uri "$MetadataUrl/vcpkg-version"

    # Start background job to periodically upload build.log to GCS for streaming logs
    $LogUploadJob = Start-Job -ScriptBlock {
        param($LogPath, $LogsBucket)
        while ($true) {
            if (Test-Path $LogPath) {
                # Use Out-Null to suppress output to avoid writing back to the transcript log
                & gcloud storage cp $LogPath "$LogsBucket/build.log" 2>&1 | Out-Null
            }
            Start-Sleep -Seconds 20
        }
    } -ArgumentList $LogPath, $LogsBucket

    Write-Host "Source Archive: $SourceArchive"
    Write-Host "Build Type: $BuildType"
    Write-Host "Features: $Features"
    Write-Host "Vcpkg Version: $VcpkgVersion"

    # 2. Check if the pre-baked vcpkg version matches the requested version
    $VcpkgDir = "C:\vcpkg"
    $BakedVersion = ""
    if (Test-Path "C:\vcpkg_version.txt") {
        $BakedVersion = (Get-Content "C:\vcpkg_version.txt").Trim()
    }
    if ($BakedVersion -ne $VcpkgVersion) {
        Write-Host "Vcpkg version mismatch! Baked: $BakedVersion, Requested: $VcpkgVersion. Re-setting up vcpkg..."
        Remove-Item -Recurse -Force $VcpkgDir -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $VcpkgDir
        $VcpkgUrl = "https://github.com/microsoft/vcpkg/archive/$VcpkgVersion.tar.gz"
        Invoke-WebRequest -Uri $VcpkgUrl -OutFile "$VcpkgDir\vcpkg.tar.gz"
        tar -xzf "$VcpkgDir\vcpkg.tar.gz" -C $VcpkgDir --strip-components=1
        & "$VcpkgDir\bootstrap-vcpkg.sh" -disableMetrics
        $VcpkgVersion | Out-File -FilePath "C:\vcpkg_version.txt" -Encoding ascii
    } else {
        Write-Host "Using pre-baked vcpkg version: $VcpkgVersion"
    }

    # 8. Extract workspace source codebase
    Write-Host "Extracting source..."
    $Workspace = "C:\workspace"
    New-Item -ItemType Directory -Force -Path $Workspace
    Set-Location $Workspace
    gcloud storage cp $SourceArchive source.tar.gz
    tar -xzf source.tar.gz
    Remove-Item source.tar.gz

    # 9. Configure environment for build
    $env:VCPKG_ROOT = $VcpkgDir
    $env:CMAKE_OUT = "C:\b"       # Directory for build output (keep it short)
    $env:EXECUTE_INTEGRATION_TESTS = "false"

    # 10. Run MSVC Developer Environment Config

    # Locates and runs vcvarsall.bat to configure compile paths
    Write-Host "Locating VS / MSVC compiler..."
    $VsInstallPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
    if (-not $VsInstallPath) {
        throw "Visual Studio installation not found!"
    }
    $VcVarsPath = Join-Path $VsInstallPath "VC\Auxiliary\Build\vcvarsall.x64.bat"
    Write-Host "Running vcvarsall.bat: $VcVarsPath"
    cmd.exe /c "`"$VcVarsPath`" && set" | Foreach-Object {
        if ($_ -match "^(.*?)=(.*)$") {
            Set-Content "env:\$($Matches[1])" $Matches[2]
        }
    }

    # 11. Run the build script using Git Bash
    $GitBashPath = "C:\Program Files\Git\bin\bash.exe"
    Write-Host "Executing windows-cmake.sh..."
    & $GitBashPath -c "ci/gha/builds/windows-cmake.sh $BuildType $Features" 2>&1
    if ($LastExitCode -ne 0) {
        throw "windows-cmake.sh failed with exit code: $LastExitCode"
    }

    # Report success status
    $Status = @{ status = "success" }
    $Status | ConvertTo-Json | Out-File -FilePath "C:\status.json" -Encoding ascii
}
catch {
    Write-Host "Error occurred during build: $_"
    $Status = @{ status = "failed"; error = $_.Exception.Message }
    $Status | ConvertTo-Json | Out-File -FilePath "C:\status.json" -Encoding ascii
}
finally {
    if ($LogUploadJob) {
        Stop-Job $LogUploadJob -ErrorAction SilentlyContinue
        Remove-Job $LogUploadJob -ErrorAction SilentlyContinue
    }
    Stop-Transcript
    if (Test-Path "C:\vcpkg\buildtrees") {
        Get-ChildItem -Path "C:\vcpkg\buildtrees" -Filter "*.log" -Recurse | ForEach-Object {
            $RelativePath = $_.FullName.Substring("C:\vcpkg\buildtrees\".Length).Replace("\", "/")
            gcloud storage cp $_.FullName "$LogsBucket/build-logs/vcpkg/$RelativePath"
        }
    }
    if (Test-Path "C:\b") {
        Get-ChildItem -Path "C:\b" -Filter "*.log" -Recurse | ForEach-Object {
            $RelativePath = $_.FullName.Substring("C:\b\".Length).Replace("\", "/")
            gcloud storage cp $_.FullName "$LogsBucket/build-logs/cmake/$RelativePath"
        }
    }
    # Upload final logs and status to GCS bucket
    gcloud storage cp C:\build.log "$LogsBucket/build.log"
    gcloud storage cp C:\status.json "$LogsBucket/status.json"
}
