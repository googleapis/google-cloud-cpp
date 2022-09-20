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
            "Will not configure a remove vcpkg cache as KOKORO_GFILE_DIR is not set."
        return
    }
    $CACHE_KEYFILE="${env:KOKORO_GFILE_DIR}/build-results-service-account.json"
    &gcloud auth activate-service-account --key-file ${CACHE_KEYFILE}
    if ($LastExitCode) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) " `
            "Will not configure a remove vcpkg cache as the service account activation failed."
        return
    }

    if (Test-Path env:GOOGLE_CLOUD_CPP_KOKORO_RESULTS) {
        $CACHE_BUCKET="${env:GOOGLE_CLOUD_CPP_KOKORO_RESULTS}"
    } else {
        $CACHE_BUCKET="cloud-cpp-kokoro-results"
    }

    if (Test-Path env:MSVC_VERSION) {
        $CACHE_FOLDER="${CACHE_BUCKET}/build-cache/google-cloud-cpp/vcpkg/windows/main/${env:MSVC_VERSION}/"
    } else {
        $CACHE_FOLDER="${CACHE_BUCKET}/build-cache/google-cloud-cpp/vcpkg/windows/main/default/"
    }
    ${env:VCPKG_BINARY_SOURCES}="x-gcs,gs://${CACHE_BUCKET}/${CACHE_FOLDER},readwrite"
}

Configure-Vcpkg-Cache
Write-Host "`n$(Get-Date -Format o) Configured vcpkg cache to use ${env:VCPKG_BINARY_SOURCES}."
