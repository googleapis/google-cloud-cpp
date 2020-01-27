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
add_executable(pubsub_handler pubsub_handler.cc gcs_indexer_constants.h)
target_link_libraries(
    pubsub_handler
    PRIVATE spanner_client
            Boost::headers
            Boost::program_options
            googleapis-c++::spanner_protos
            gRPC::grpc++
            gRPC::grpc
            ${EXTRA_LIBRARY}
            OpenSSL::Crypto
            OpenSSL::SSL
            ZLIB::ZLIB)

add_executable(create_database create_database.cc gcs_indexer_constants.h)
target_link_libraries(
    create_database
    PRIVATE spanner_client
            Boost::headers
            Boost::program_options
            googleapis-c++::spanner_protos
            gRPC::grpc++
            gRPC::grpc
            ${EXTRA_LIBRARY}
            OpenSSL::Crypto
            OpenSSL::SSL
            ZLIB::ZLIB)

add_executable(refresh_bucket refresh_bucket.cc gcs_indexer_constants.h)
target_link_libraries(
    refresh_bucket
    PRIVATE spanner_client
            storage_client
            Boost::headers
            Boost::program_options
            googleapis-c++::spanner_protos
            gRPC::grpc++
            gRPC::grpc
            CURL::libcurl
            ${EXTRA_LIBRARY}
            OpenSSL::Crypto
            OpenSSL::SSL
            ZLIB::ZLIB)

add_executable(generate_randomly_named_objects
               generate_randomly_named_objects.cc)
target_link_libraries(
    generate_randomly_named_objects
    PRIVATE storage_client
            Boost::headers
            Boost::program_options
            CURL::libcurl
            ${EXTRA_LIBRARY}
            OpenSSL::Crypto
            OpenSSL::SSL
            ZLIB::ZLIB)

add_executable(delete_bucket_index delete_bucket_index.cc)
target_link_libraries(
    delete_bucket_index
    PRIVATE spanner_client
            Boost::headers
            Boost::program_options
            googleapis-c++::spanner_protos
            gRPC::grpc++
            gRPC::grpc
            ${EXTRA_LIBRARY}
            OpenSSL::Crypto
            OpenSSL::SSL
            ZLIB::ZLIB)

include(GNUInstallDirs)
install(TARGETS pubsub_handler create_database generate_randomly_named_objects
                refresh_bucket delete_bucket_index
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
