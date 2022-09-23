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

# Helper functions to configure CMake


function Create-CMakeOut {
    # Create output directory for CMake. Our builds can create really long paths,
    # sometimes exceeding the Windows limits. Using a short name for the root of
    # the CMake output directory works around this problem.
    if (Test-Path "T:") {
        # In the Kokoro environment "T:" is a faster drive.
        $cmake_out = "T:/cm"
    } else {
        $cmake_out = "${env:SystemDrive}/cm"
    }
    if (-not (Test-Path $cmake_out)) {
        Write-Host -ForegroundColor Yellow "`n$(Get-Date -Format o) Create CMake Output (${cmake_out})"
        New-Item -ItemType Directory -Path ${cmake_out} | Out-Null
    }
    return $cmake_out
}
