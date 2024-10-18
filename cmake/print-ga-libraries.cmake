# ~~~
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
# ~~~

# A CMake script to print the GA features.
cmake_minimum_required(VERSION 3.13...3.24)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
include(GoogleCloudCppFeatures)

foreach (feature IN LISTS GOOGLE_CLOUD_CPP_GA_LIBRARIES
                          GOOGLE_CLOUD_CPP_TRANSITION_LIBRARIES)
    if (${feature} STREQUAL "compute")
        # The `compute` feature is a collection of many `compute_*` libraries.
        # We want to enumerate them here.
        foreach (library IN LISTS GOOGLE_CLOUD_CPP_COMPUTE_LIBRARIES)
            message(${library})
        endforeach ()
    else ()
        message(${feature})
    endif ()
endforeach ()
