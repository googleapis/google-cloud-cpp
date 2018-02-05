#!/usr/bin/env powershell

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

# Stop on errors. This has a similar effect as `set -e` on Unix shells.
$ErrorActionPreference = "Stop"

# Using relative paths works both on appveyor and in development workstations.
cd ..

# Update or clone the 'vcpkg' package manager.
if (Test-Path vcpkg\.git) {
  cd vcpkg
  git pull
} elseif (Test-Path vcpkg\installed) {
  move vcpkg vcpkg-tmp
  git clone https://github.com/Microsoft/vcpkg
  move vcpkg-tmp\installed vcpkg
  cd vcpkg
} else {
  git clone https://github.com/Microsoft/vcpkg
  cd vcpkg
}
if ($LASTEXITCODE) {
  throw "git setup failed with exit code $LASTEXITCODE"
}

# Build the tool each time, it is fast to do so.
powershell -exec bypass scripts\bootstrap.ps1
if ($LASTEXITCODE) {
  throw "vcpkg bootstrap failed with exit code $LASTEXITCODE"
}

# Integrate installed packages into the build environment.
.\vcpkg integrate install
if ($LASTEXITCODE) {
  throw "vcpkg integrate failed with exit code $LASTEXITCODE"
}

# AppVeyor limits builds to 60 minutes. Building all the dependencies takes
# longer than that. Cache the dependencies to work around the build time
# restrictions. Explicitly install each dependency because if we run out of
# time in the AppVeyor build the cache is at least partially refreshed, a
# rebuild will start with some dependencies already cached, and likely
# complete before the AppVeyor build time limit.
$packages = @("zlib:x64-windows-static", "openssl:x64-windows-static",
              "protobuf:x64-windows-static", "c-ares:x64-windows-static",
              "grpc:x64-windows-static", "cctz:x64-windows-static",
              "absl:x64-windows-static")
foreach ($pkg in $packages) {
  $cmd = ".\vcpkg.exe install $pkg"
  Invoke-Expression $cmd
  if ($LASTEXITCODE) {
    throw "vcpkg install $pkg failed with exit code $LASTEXITCODE"
  }
}

cd ..
