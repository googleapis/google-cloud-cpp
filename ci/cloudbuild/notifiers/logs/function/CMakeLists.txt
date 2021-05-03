# ~~~
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
# ~~~

set(CMAKE_CXX_STANDARD 17)

find_package(fmt REQUIRED)
find_package(functions_framework_cpp REQUIRED)
find_package(google_cloud_cpp_storage REQUIRED)

add_library(functions_framework_cpp_function function.cc generate_svg_badge.cc)
target_link_libraries(
    functions_framework_cpp_function functions-framework-cpp::framework
    fmt::fmt google-cloud-cpp::storage stdc++fs)
target_include_directories(functions_framework_cpp_function
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(print_svg_badge generate_svg_badge.cc print_svg_badge.cc)
target_link_libraries(print_svg_badge PRIVATE fmt::fmt stdc++fs)
target_include_directories(print_svg_badge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
