# !/usr/bin/env powershell
# Copyright 2020 Google LLC
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

# Stop on errors. This is similar to `set -e` on Unix shells.
$ErrorActionPreference = "Stop"

# Install dependencies for GCS emulator.
pip install setuptools wheel
pip install git+git://github.com/googleapis/python-storage@8cf6c62a96ba3fff7e5028d931231e28e5029f1c
pip install flask==1.1.2 httpbin==0.7.0 scalpl==0.4.0 crc32c==2.1 gevent==20.9.0 gunicorn==20.0.4
