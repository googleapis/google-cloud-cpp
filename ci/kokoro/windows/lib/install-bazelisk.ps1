#!/usr/bin/env powershell
#
# Copyright 2021 Google LLC
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

# Stop on errors. This is similar to `set -e` on Unix shells.
$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path "C:\bin"

ForEach($_ in (30, 60, 120, 240, 0)) {
    Write-Host -ForegroundColor Yellow "Downloading bazelisk.exe to C:\Windows"
    try {
        (New-Object System.Net.WebClient).Downloadfile('https://github.com/bazelbuild/bazelisk/releases/download/v1.10.1/bazelisk-windows-amd64.exe', 'C:\bin\bazelisk.exe');
        Write-Host -ForegroundColor Green "bazelisk successfully downloaded"
        Exit 0
    } catch {
    }
    Start-Sleep -Seconds $_
}

Write-Host -ForegroundColor Red "bazelisk download failed"
Exit 1
