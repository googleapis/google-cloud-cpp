@REM Copyright 2020 Google LLC
@REM
@REM Licensed under the Apache License, Version 2.0 (the "License");
@REM you may not use this file except in compliance with the License.
@REM You may obtain a copy of the License at
@REM
@REM     https://www.apache.org/licenses/LICENSE-2.0
@REM
@REM Unless required by applicable law or agreed to in writing, software
@REM distributed under the License is distributed on an "AS IS" BASIS,
@REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
@REM See the License for the specific language governing permissions and
@REM limitations under the License.

REM Install Bazelisk.
@echo %date% %time%
@cd github\google-cloud-cpp
@powershell -exec bypass ci\kokoro\windows\install-bazelisk.ps1
@if ERRORLEVEL 1 exit /b 1

REM Change PATH to install the Bazelisk version we just installed
@set PATH=C:\bin;%PATH%

REM Configure the environment to use MSVC %MSVC_VERSION% and then switch to PowerShell.
if "%KOKORO_JOB_POOL%" == "yoshi-cpp-win" (
  call "c:\Program Files (x86)\Microsoft Visual Studio\%MSVC_VERSION%\Community\VC\Auxiliary\Build\vcvars32.bat"
  set BAZEL_VC="C:\Program Files\Microsoft Visual Studio\%MSVC_VERSION%\Community\VC"
) else (
  call "c:\Program Files (x86)\Microsoft Visual Studio\%MSVC_VERSION%\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
  set BAZEL_VC="C:\Program Files\Microsoft Visual Studio\%MSVC_VERSION%\BuildTools\VC"
)

REM The remaining of the build script is implemented in PowerShell.
@echo %date% %time%
powershell -exec bypass ci\kokoro\windows\build.ps1
@if ERRORLEVEL 1 exit /b 1

@echo DONE "============================================="
@echo %date% %time%
