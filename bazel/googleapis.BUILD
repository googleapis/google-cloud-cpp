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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

# This build file overlays on top of the BUILD files for the googleapis repo,
# and it adds a target that lets us include their header files using
# angle-brackets, thus treating their headers as system includes. This allows
# us to dial-up the warnings in our own code, without seeing compiler warnings
# from their headers, which we do not own.
cc_library(
    name = "googleapis_system_includes",
    includes = [
        ".",
    ],
)
