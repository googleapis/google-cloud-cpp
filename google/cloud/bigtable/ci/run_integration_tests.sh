#!/usr/bin/env bash
#
# Copyright 2018 Google LLC
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

# This script should is called from the build directory, and it finds other
# scripts in the source directory using its own path.
readonly BINDIR="$(dirname "$0")"
source "${BINDIR}/../../../../ci/colors.sh"

readonly BTDIR="google/cloud/bigtable"

# TODO(#3526) - remove this file and call sites when dust settles
