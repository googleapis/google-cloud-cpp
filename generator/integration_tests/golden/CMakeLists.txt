# ~~~
# Copyright 2020 Google LLC
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

cmake_minimum_required(VERSION 3.5)

# Disable clang-tidy for these testing files
add_library(target_no_static_code_analysis OBJECT
            database_admin_stub.gcpcxx.pb.h)

set_target_properties(target_no_static_code_analysis PROPERTIES CXX_CLANG_TIDY
                                                                "")

set_target_properties(target_no_static_code_analysis PROPERTIES LINKER_LANGUAGE
                                                                CXX)
