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

# Helper functions to configure the vcpkg cache

function Configure-Vcpkg-Cache {
    if (-not (Test-Path env:KOKORO_GFILE_DIR)) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Will not configure a remote vcpkg cache as KOKORO_GFILE_DIR is not set."
        return
    }
    $CACHE_KEYFILE="${env:KOKORO_GFILE_DIR}/build-results-service-account.json"
    &gcloud auth activate-service-account --key-file ${CACHE_KEYFILE}
    if ($LastExitCode) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Will not configure a remote vcpkg cache as the service account activation failed."
        return
    }

    if (Test-Path env:GOOGLE_CLOUD_CPP_KOKORO_RESULTS) {
        $CACHE_BUCKET="${env:GOOGLE_CLOUD_CPP_KOKORO_RESULTS}"
    } else {
        $CACHE_BUCKET="cloud-cpp-kokoro-results"
    }

    if (Test-Path env:MSVC_VERSION) {
        $CACHE_FOLDER="build-cache/google-cloud-cpp/vcpkg/windows/main/${env:MSVC_VERSION}/"
    } else {
        $CACHE_FOLDER="build-cache/google-cloud-cpp/vcpkg/windows/main/default/"
    }
    ${env:VCPKG_BINARY_SOURCES}="x-gcs,gs://${CACHE_BUCKET}/${CACHE_FOLDER},readwrite"
    Write-Host "`n$(Get-Date -Format o) Configured vcpkg cache to use ${env:VCPKG_BINARY_SOURCES}."
}

function Install-Vcpkg {
    # quickstart builds install `google-cloud-cpp`. Therefore, these builds have all the
    # headers for (potentially) older versions of `google-cloud-cpp` in the include search
    # path. In the CI environment only one type of build happens at a time, but manual tests
    # create both. We separate the vcpkg installation for those builds to avoid confusion.
    param([string]$project_root, [string]$suffix)

    # Create a directory to install vcpkg. Our builds can create really long paths,
    # sometimes exceeding the Windows limits. Using a short name for the root of
    # the CMake output directory works around this problem.
    if (Test-Path "T:") {
        # In the Kokoro environment "T:" is a faster drive.
        $vcpkg_root = "T:/vcpkg${suffix}"
        $extract_root = "T:"
    } else {
        $vcpkg_root = "${env:SystemDrive}/vcpkg${suffix}"
        $extract_root = "${env:SystemDrive}"
    }
    # In manual builds the directory already exists, assume it is a good directory and return
    if (Test-Path "${vcpkg_root}") {
        Write-Host -ForegroundColor Green "$(Get-Date -Format o) vcpkg directory already exists."
        return "${vcpkg_root}"
    }

    $vcpkg_version = Get-Content -Path "${project_root}\ci\etc\vcpkg-version.txt"
    $vcpkg_url = "https://github.com/microsoft/vcpkg/archive/${vcpkg_version}.zip"
    if ($vcpkg_version -match "[0-9]{4}.[0-9]{2}.[0-9]{2}" ) {
        $vcpkg_url = "https://github.com/microsoft/vcpkg/archive/refs/tags/${vcpkg_version}.zip"
    }
    Write-Host "$(Get-Date -Format o) Downloading vcpkg archive from $vcpkg_url"

    # Download the right version of `vcpkg`
    ForEach($_ in (1, 2, 3)) {
        if ($_ -ne 1) { Start-Sleep -Seconds (60 * $_) }
        Write-Host "$(Get-Date -Format o) Downloading vcpkg ports archive [$_]"
        try {
            (New-Object System.Net.WebClient).Downloadfile($vcpkg_url,
                    "${env:TEMP}/${vcpkg_version}.zip") |  Write-Host
            break
        } catch {
            Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) download error for $vcpkg_url"
        }
    }
    7z x -o"${extract_root}" "${env:TEMP}/${vcpkg_version}.zip" -aoa -bsp0 | Write-Host
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "`n$(Get-Date -Format o) " `
            "extracting vcpkg from archive failed with exit code ${LastExitCode}."
        Exit 1
    }
    Write-Host "$(Get-Date -Format o) rename ${extract_root}/vcpkg-${vcpkg_version} -> ${vcpkg_root}"
    Rename-Item "${extract_root}/vcpkg-${vcpkg_version}" "${vcpkg_root}"

    if (-not (Test-Path "${vcpkg_root}")) {
        Write-Host -ForegroundColor Red "Missing vcpkg root directory (${vcpkg_root})."
        Exit 1
    }

    # On Windows the bootstrap script simply downloads the latest release.
    # We need to retry the operation to prevent flakes, but it is relatively
    # fast.
    ForEach($_ in (1, 2, 3)) {
        if (Test-Path "${vcpkg_root}/vcpkg.exe") {
            break
        }
        if ($_ -ne 1) { Start-Sleep -Seconds (60 * $_) }
        Write-Host "$(Get-Date -Format o) bootstrap vcpkg in ${vcpkg_root}."
        try {
            &"${vcpkg_root}/bootstrap-vcpkg.bat" | Write-Host
        } catch {
            Write-Host -ForegroundColor Yellow "$(Get-Date -Format o) bootstrap error"
        }
    }
    if (-not (Test-Path "${vcpkg_root}/vcpkg.exe")) {
        Write-Host -ForegroundColor Red "Missing vcpkg executable (${vcpkg_root}/vcpkg.exe)."
        Exit 1
    }

    # Download the tools that vcpkg typically needs
    ForEach($tool in ("cmake", "ninja", "7zip")) {
        ForEach($_ in (1, 2, 3)) {
            if ($_ -ne 1) { Start-Sleep -Seconds (60 * $_) }
            Write-Host "$(Get-Date -Format o) Fetch ${tool} [$_]"
            &"${vcpkg_root}/vcpkg.exe" fetch "${tool}" | Write-Host
            if ($LastExitCode -eq 0) {
                break
            }
        }
    }

    return "${vcpkg_root}"
}

function Build-Vcpkg-Packages {
    param([string]$vcpkg_root, [string[]]$packages)

    $vcpkg_flags = @(
        "--triplet", "${env:VCPKG_TRIPLET}",
        "--feature-flags=-manifests"
    )

    # Remove old versions of the packages.
    Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Cleanup outdated vcpkg packages."
    &"${vcpkg_root}/vcpkg.exe" remove ${vcpkg_flags} --outdated --recurse
    if ($LastExitCode) {
        Write-Host -ForegroundColor Red "vcpkg remove --outdated failed with exit code $LastExitCode"
        Exit ${LastExitCode}
    }

    # Install the packages one at a time.
    Write-Host "$(Get-Date -Format o) Building vcpkg packages."
    foreach ($pkg in $packages) {
        ForEach($_ in (1, 2, 3)) {
            if ($_ -ne 1) { Start-Sleep -Seconds (60 * $_) }
            Write-Host "$(Get-Date -Format o) vcpkg install ${pkg} [$_]"
            &"${vcpkg_root}/vcpkg.exe" install ${vcpkg_flags} --recurse "${pkg}"
            if ($LastExitCode -eq 0) {
                break
            }
            $pkg_base = $pkg -replace '\[.*', ''
            Write-Host -ForegroundColor Yellow "`n`n$(Get-Date -Format o) DEBUG DEBUG DEBUG - dbg-out"
            Get-Content "${vcpkg_root}/buildtrees/${pkg_base}/install-${env:VCPKG_TRIPLET}-dbg-out.log" -ErrorAction SilentlyContinue | Write-Host
            Write-Host -ForegroundColor Yellow "`n`n$(Get-Date -Format o) DEBUG DEBUG DEBUG - dbg-err"
            Get-Content "${vcpkg_root}/buildtrees/${pkg_base}/install-${env:VCPKG_TRIPLET}-dbg-err.log" -ErrorAction SilentlyContinue | Write-Host
            Write-Host -ForegroundColor Yellow "`n`n$(Get-Date -Format o) DEBUG DEBUG DEBUG - rel-out"
            Get-Content "${vcpkg_root}/buildtrees/${pkg_base}/install-${env:VCPKG_TRIPLET}-rel-out.log" -ErrorAction SilentlyContinue | Write-Host
            Write-Host -ForegroundColor Yellow "`n`n$(Get-Date -Format o) DEBUG DEBUG DEBUG - rel-err"
            Get-Content "${vcpkg_root}/buildtrees/${pkg_base}/install-${env:VCPKG_TRIPLET}-rel-err.log" -ErrorAction SilentlyContinue | Write-Host
        }
        if ($LastExitCode -ne 0) {
            Write-Host -ForegroundColor Red "vcpkg install ${pkg} failed with exit code ${LastExitCode}"
            Exit ${LastExitCode}
        }
    }

    Write-Host "`n$(Get-Date -Format o) vcpkg list"
    &"${vcpkg_root}/vcpkg.exe" list ${vcpkg_flags}
}

Configure-Vcpkg-Cache
