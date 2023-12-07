# ~~~
# Copyright 2022 Google LLC
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
# ~~~

if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
    AND (Protobuf_VERSION VERSION_LESS 24.0))
    message(
        WARNING "Cannot build google/cloud/channel with"
                " Protobuf < 24.0 and GCC < 8.0 - the protos have enum"
                " names that conflict with some system / language macros."
                " GCC < 8.0 exposes these macros in C++ headers."
                " Protobuf < 24.0 does not include the workarounds for"
                " these conflicts.")
    return()
endif ()

include(GoogleCloudCppLibrary)

google_cloud_cpp_add_gapic_library(channel "Cloud Channel API"
                                   SERVICE_DIRS "__EMPTY__" "v1/")

# The quickstart is too difficult to run successfully, hence, there is no
# quickstart runner.
