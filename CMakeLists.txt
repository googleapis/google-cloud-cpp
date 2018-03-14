# Copyright 2017 Google Inc.
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

cmake_minimum_required(VERSION 3.5)

# Define the project name and where to report bugs.
set(PACKAGE_BUGREPORT "https://github.com/GoogleCloudPlatform/google-cloud-cpp/issues")
project(google-cloud-cpp CXX C)

set(PROJECT_THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party")

# Add gRPC and protobuf targets as submodules or packages, depending on the
# configuration.  This should happen before enable_testing(), because we are
# not interested in compiling and running the gRPC tests.
include (${PROJECT_SOURCE_DIR}/cmake/IncludeGrpc.cmake)

# Enable testing in this directory so we can do a top-level `make test`.
enable_testing()

# Each subproject adds dependencies to this target to have their docs generated.
add_custom_target(doxygen-docs)

# Add subprojects here.
add_subdirectory(bigtable)

# These are tests to reproduce problems in gRPC and other dependencies.
add_subdirectory(tests/grpc-crash)
add_subdirectory(tests/grpc-round-robin-freeze)
