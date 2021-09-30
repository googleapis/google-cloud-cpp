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

# This program should print zero or more key/value pairs to standard
# output, one entry on each line, and then exit with zero (otherwise
# the build fails). The key names can be anything but they may only use
# upper case letters and underscores. The first space after the key
# name separates it from the value. The value is the rest of the line
# (including additional whitespaces). Neither the key nor the value may
# span multiple lines. Keys must not be duplicated.

echo "STABLE_GIT_COMMIT $(git rev-parse --short HEAD || echo "unknown")"
