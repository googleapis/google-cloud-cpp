#!/usr/bin/env bash
# Copyright 2019 Google LLC
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

# This script is sourced from several other scripts in `ci/` to configure the
# repository name (and short name). It makes those scripts portable across
# `google-cloud-cpp*` repositories.

GOOGLE_CLOUD_CPP_REPOSITORY="google-cloud-cpp"
readonly GOOGLE_CLOUD_CPP_REPOSITORY

GOOGLE_CLOUD_CPP_REPOSITORY_SHORT="cpp"
readonly GOOGLE_CLOUD_CPP_REPOSITORY_SHORT
