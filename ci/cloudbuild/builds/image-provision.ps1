# Setup log output redirection
$LogPath = "C:\image-provision.log"
Start-Transcript -Path $LogPath

try {
    # 1. Ensure Chocolatey is installed
    if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
        if (-not (Test-Path "C:\ProgramData\chocolatey\bin\choco.exe")) {
            Write-Host "Installing Chocolatey..."
            Set-ExecutionPolicy Bypass -Scope Process -Force
            [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
            Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
        }
        $env:Path += ";C:\ProgramData\chocolatey\bin"
    }

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

    # 4. Ensure Ninja is installed and in the PATH
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        Write-Host "Installing Ninja..."
        $NinjaDir = "C:\ninja"
        New-Item -ItemType Directory -Force -Path $NinjaDir
        Invoke-WebRequest -Uri "https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip" -OutFile "$NinjaDir\ninja.zip"
        Expand-Archive -Path "$NinjaDir\ninja.zip" -DestinationPath $NinjaDir -Force
        Remove-Item "$NinjaDir\ninja.zip"
        $env:Path += ";$NinjaDir"
    }

    # 5. Download and Install sccache
    if (-not (Get-Command sccache -ErrorAction SilentlyContinue)) {
        Write-Host "Installing sccache..."
        $SccacheDir = "C:\sccache"
        New-Item -ItemType Directory -Force -Path $SccacheDir
        $SccacheUrl = "https://github.com/mozilla/sccache/releases/download/v0.9.1/sccache-v0.9.1-x86_64-pc-windows-msvc.tar.gz"
        Invoke-WebRequest -Uri $SccacheUrl -OutFile "$SccacheDir\sccache.tar.gz"
        tar -xzf "$SccacheDir\sccache.tar.gz" -C $SccacheDir --strip-components=1
        $env:Path += ";$SccacheDir"
    }

    # Set machine-level persistent PATH for Git, CMake, Ninja, sccache
    [Environment]::SetEnvironmentVariable("Path", $env:Path, [EnvironmentVariableTarget]::Machine)

    # 6. Download and bootstrap vcpkg
    Write-Host "Setting up vcpkg..."
    $VcpkgDir = "C:\vcpkg"
    New-Item -ItemType Directory -Force -Path $VcpkgDir
    # Default version to bake in: matching the repository version in ci/etc/vcpkg-version.txt
    $VcpkgVersion = "3895230f38e498525f2560a281223d12066fa74a"
    $VcpkgUrl = "https://github.com/microsoft/vcpkg/archive/$VcpkgVersion.tar.gz"
    Invoke-WebRequest -Uri $VcpkgUrl -OutFile "$VcpkgDir\vcpkg.tar.gz"
    tar -xzf "$VcpkgDir\vcpkg.tar.gz" -C $VcpkgDir --strip-components=1
    & "$VcpkgDir\bootstrap-vcpkg.sh" -disableMetrics
    
    # Save the baked vcpkg version to a file
    $VcpkgVersion | Out-File -FilePath "C:\vcpkg_version.txt" -Encoding ascii
    [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $VcpkgDir, [EnvironmentVariableTarget]::Machine)

    # 7. Install Visual Studio 2022 Build Tools
    if (-not (Test-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe")) {
        Write-Host "Installing Visual Studio 2022 Build Tools..."
        $VsBootstrapperPath = "C:\vs_buildtools.exe"
        Invoke-WebRequest -Uri "https://aka.ms/vs/17/release/vs_buildtools.exe" -OutFile $VsBootstrapperPath
        
        Write-Host "Running Visual Studio Installer..."
        $Process = Start-Process -FilePath $VsBootstrapperPath -ArgumentList "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --norestart --wait" -Wait -NoNewWindow -PassThru
        
        if ($Process.ExitCode -ne 0 -and $Process.ExitCode -ne 3010) {
            throw "Visual Studio Build Tools installation failed with exit code: $($Process.ExitCode)"
        }
        Write-Host "Visual Studio Build Tools installed successfully."
    }

    # 8. Run Sysprep generalization and shutdown
    Write-Host "Running Sysprep generalization and shutdown..."
    & $env:SystemRoot\System32\Sysprep\Sysprep.exe /oobe /generalize /shutdown /quiet
}
catch {
    Write-Error "Error occurred during image provisioning: $_"
    exit 1
}
finally {
    Stop-Transcript
}
