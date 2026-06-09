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

    Write-Host "Source Archive: $SourceArchive"
    Write-Host "Build Type: $BuildType"
    Write-Host "Features: $Features"
    Write-Host "Vcpkg Version: $VcpkgVersion"

    # 2. Ensure Git is installed and in the PATH
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        Write-Host "Installing Git..."
        choco install -y git --no-progress
        $env:Path += ";C:\Program Files\Git\cmd"
    }

    # 3. Ensure CMake is installed and in the PATH
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Host "Installing CMake..."
        choco install -y cmake --version 3.31.6 --no-progress
        $env:Path += ";C:\Program Files\CMake\bin"
    }

    # 4. Download and Install sccache
    Write-Host "Installing sccache..."
    $SccacheDir = "C:\sccache"
    New-Item -ItemType Directory -Force -Path $SccacheDir
    $SccacheUrl = "https://github.com/mozilla/sccache/releases/download/v0.9.1/sccache-v0.9.1-x86_64-pc-windows-msvc.tar.gz"
    Invoke-WebRequest -Uri $SccacheUrl -OutFile "$SccacheDir\sccache.tar.gz"
    tar -xzf "$SccacheDir\sccache.tar.gz" -C $SccacheDir --strip-components=1
    $env:Path += ";$SccacheDir"

    # 5. Download and bootstrap vcpkg
    Write-Host "Setting up vcpkg..."
    $VcpkgDir = "C:\vcpkg"
    New-Item -ItemType Directory -Force -Path $VcpkgDir
    $VcpkgUrl = "https://github.com/microsoft/vcpkg/archive/$VcpkgVersion.tar.gz"
    Invoke-WebRequest -Uri $VcpkgUrl -OutFile "$VcpkgDir\vcpkg.tar.gz"
    tar -xzf "$VcpkgDir\vcpkg.tar.gz" -C $VcpkgDir --strip-components=1
    & "$VcpkgDir\bootstrap-vcpkg.sh" -disableMetrics

    # 6. Extract workspace source codebase
    Write-Host "Extracting source..."
    $Workspace = "C:\workspace"
    New-Item -ItemType Directory -Force -Path $Workspace
    Set-Location $Workspace
    gcloud storage cp $SourceArchive source.tar.gz
    tar -xzf source.tar.gz
    Remove-Item source.tar.gz

    # 7. Configure environment for build
    $env:VCPKG_ROOT = $VcpkgDir
    $env:CMAKE_OUT = "C:\b"       # Directory for build output (keep it short)
    $env:EXECUTE_INTEGRATION_TESTS = "false"

    # 8. Run MSVC Developer Environment Config
    # Locates and runs vcvarsall.bat to configure compile paths
    Write-Host "Locating VS / MSVC compiler..."
    $VsInstallPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
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

    # 9. Run the build script using Git Bash
    $GitBashPath = "C:\Program Files\Git\bin\bash.exe"
    Write-Host "Executing windows-cmake.sh..."
    & $GitBashPath -c "ci/gha/builds/windows-cmake.sh $BuildType $Features"

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
    Stop-Transcript
    # Upload final logs and status to GCS bucket
    gcloud storage cp C:\build.log "$LogsBucket/build.log"
    gcloud storage cp C:\status.json "$LogsBucket/status.json"
}
