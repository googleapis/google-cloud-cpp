# !/ usr / bin / env powershell

# Copyright 2017 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Stop on errors, similar "set -e" on Unix shells...
$ErrorActionPreference = "Stop"

$dir = Split-Path (Get-Item -Path ".\" -Verbose).FullName
if (Test-Path variable:env:APPVEYOR_BUILD_FOLDER) {
  $dir = Split-Path $env:APPVEYOR_BUILD_FOLDER
}

$integrate = "$dir\vcpkg\vcpkg.exe integrate install"
Invoke-Expression $integrate
if ($LASTEXITCODE) {
  throw "vcpkg integrate failed with exit code $LASTEXITCODE"
}

mkdir build
cd build
echo $dir
cmake -DCMAKE_TOOLCHAIN_FILE="$dir\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x86-windows-static .. -DGOOGLE_CLOUD_CPP_GRPC_PROVIDER=package -DGTEST_USE_OWN_TR1_TUPLE=1

if ($LASTEXITCODE) {
  throw "cmake failed with exit code $LASTEXITCODE"
}

cmake --build .
if ($LASTEXITCODE) {
  throw "cmake build failed with exit code $LASTEXITCODE"
}
