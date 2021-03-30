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

export CC=gcc
export CXX=g++

cmake -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=yes \
  -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON \
  -Hsuper -Bcmake-out
cmake --build cmake-out
# Testing is automatically done by the super build itself. See
# https://github.com/googleapis/google-cloud-cpp/blob/master/super/CMakeLists.txt
