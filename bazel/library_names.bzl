# Copyright 2023 Google LLC
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

def library_dir_name(library):
    if library.startswith("compute_"):
        return "compute"
    return library

def mocks_filegroup_name(library):
    if library.startswith("compute_"):
        return library.removeprefix("compute_") + "_mock_hdrs"
    return "mocks"

def hdrs_filegroup_name(library):
    if library.startswith("compute_"):
        return library.removeprefix("compute_") + "_public_hdrs"
    return "public_hdrs"
