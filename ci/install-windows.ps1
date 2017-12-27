#!/usr/bin/env powershell
#
#Copyright 2017 Google Inc.
#
#Licensed under the Apache License, Version 2.0(the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#http:  // www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

# Stop on errors, similar (but not quite as useful as) "set -e" on Unix shells ...
$ErrorActionPreference = "Stop"

# Using relative paths works both on appveyor and in development workstations
cd ..

# ... update or clone the vcpkg package manager ...
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

# ... install cmake because the version in appveyor is too old for some of
# the packages ...
choco install -y cmake cmake.portable
if ($LASTEXITCODE) {
  throw "choco install cmake failed with exit code $LASTEXITCODE"
}

# ... build the tool each time, it is fast to do so ...
powershell -exec bypass scripts\bootstrap.ps1
if ($LASTEXITCODE) {
  throw "vcpkg bootstrap failed with exit code $LASTEXITCODE"
}

# ... integrate installed packages into the build environment ...
.\vcpkg integrate install
if ($LASTEXITCODE) {
  throw "vcpkg integrate failed with exit code $LASTEXITCODE"
}

# ... if necessary, install grpc again.  Normally the packages are
# cached by the CI system (appveyor) so this is not too painful.
# We explicitly install each dependency because if we run out of time
# in the appveyor build the cache is at least partially refreshed and
# a rebuild will complete creating the cache ...
$packages = @("zlib:x86-windows-static", "openssl:x86-windows-static",
              "protobuf:x86-windows-static", "c-ares:x86-windows-static",
              "grpc:x86-windows-static")
foreach ($pkg in $packages) {
  $cmd = ".\vcpkg.exe install $pkg"
  Invoke-Expression $cmd
  if ($LASTEXITCODE) {
    throw "vcpkg install $pkg failed with exit code $LASTEXITCODE"
  }
}

cd ..\google-cloud-cpp
