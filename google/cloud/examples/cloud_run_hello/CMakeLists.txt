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

cmake_minimum_required(VERSION 3.10)

# Define the project name and where to report bugs.
set(PACKAGE_BUGREPORT "https://github.com/coryan/docker-grpc-cpp/issues")
project(Docker-Run-C++ CXX C)

# Configure the Compiler options, we will be using C++17 features.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(spanner_client REQUIRED)
find_package(storage_client REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost 1.66 REQUIRED COMPONENTS program_options)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
set(EXTRA_LIBRARY
    ""
    CACHE
        FILEPATH
        "Additional libraries.
        Several CMake modules (but not CONFIGs!) do not link the dependencies
        when using static libraries. This flag can be used to add additional
        from the command line (or a Dockerfile).")
mark_as_advanced(EXTRA_LIBRARY)

# When using static libraries the FindgRPC.cmake module does not define the
# correct dependencies (OpenSSL::Crypto, c-cares, etc) for gRPC::grpc.
# Explicitly listing these dependencies avoids the undefined symbols problems.
add_executable(cloud_run_hello cloud_run_hello.cc)
target_link_libraries(
    cloud_run_hello
    PRIVATE spanner_client
            Boost::headers
            Boost::program_options
            OpenSSL::Crypto
            OpenSSL::SSL
            ZLIB::ZLIB)

include(GNUInstallDirs)
install(TARGETS cloud_run_hello
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
