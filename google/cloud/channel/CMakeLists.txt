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

# TODO(#8125): enable on GCC < 8.0
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION
                                                  VERSION_LESS 8.0)
    message(WARNING "Cannot build google/cloud/channel with GCC < 8.0.")
    return()
endif ()

include(GoogleCloudCppLibrary)

set(GOOGLE_CLOUD_CPP_SERVICE_DIRS "" "v1/")

google_cloud_cpp_add_ga_grpc_library(channel "Cloud Channel API")

# The quickstart is too difficult to run successfully, hence, there is no
# quickstart runner.
