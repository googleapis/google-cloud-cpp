#!/bin/bash
#
# Copyright 2021 Google LLC
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

set -eu

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/lib/io.sh

export CC=clang
export CXX=clang++
# For docuploader to work
export LC_ALL=C.UTF-8
export LANG=C.UTF-8

cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -S . -B cmake-out
cmake --build cmake-out
env -C cmake-out ctest -LE "integration-test"

io::log_h1 "Generating doxygen docs"
cmake --build cmake-out --target doxygen-docs

io::log_h1 "Uploading docs"
io::log "Installing docuploader package"
python3 -m pip install --user gcp-docuploader protobuf --upgrade
io::log "Creating metadata"
python3 -m docuploader create-metadata \
  --name "jgm-testing" \
  --version "jgm-version" \
  --language cpp

io::log "Uploading docs"
python3 -m docuploader upload cmake-out/google/cloud/spanner/html \
  --staging-bucket jgm-cloud-cxx-bucket
