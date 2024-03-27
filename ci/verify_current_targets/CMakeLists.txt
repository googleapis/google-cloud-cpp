# ~~~
# Copyright 2021 Google LLC
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

cmake_minimum_required(VERSION 3.10...3.24)
project(verify-exported-targets CXX)

# We list the common libraries first because we want to load their packages
# early.
set(common_libraries common grpc_utils mocks rest_internal)

# Define GA libraries.
set(ga_libraries
    # cmake-format: sort
    bigquery
    bigtable
    bigtable_mocks
    iam
    iam_mocks
    logging
    opentelemetry
    pubsub
    pubsub_mocks
    spanner
    spanner_mocks
    storage
    storage_grpc
    storage_grpc_mocks)

# Define experimental libraries. The only difference is the name of the CMake
# targets.
set(experimental_libraries # cmake-format: sort
                           bigquery_rest)

# CMake can use pkg-config to find dependencies. We recommend using CMake
# targets, but we want to verify our pkg-config files remain usable and
# backwards compatible.
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
include(CTest)

function (add_test_case name)
    add_executable("${name}" verify_current_targets.cc)
    target_link_libraries("${name}" PRIVATE ${ARGN})
    add_test(NAME "${name}" COMMAND "${name}")
endfunction ()

foreach (library ${common_libraries} ${ga_libraries} ${experimental_libraries})
    find_package("google_cloud_cpp_${library}" REQUIRED)
endforeach ()

foreach (library ${common_libraries} ${ga_libraries})
    add_test_case(test_cmake_${library} google-cloud-cpp::${library})
endforeach ()

foreach (library ${experimental_libraries})
    add_test_case(test_cmake_${library}
                  google-cloud-cpp::experimental-${library})
endforeach ()

foreach (library ${common_libraries} ${ga_libraries} ${experimental_libraries})
    message("library=${library}")
    pkg_check_modules(${library} IMPORTED_TARGET REQUIRED
                      google_cloud_cpp_${library})
    add_test_case(test_pc_${library} PkgConfig::${library})
endforeach ()

# Verify backwards compatible proto libraries are installed. See #8022 for more
# details.
foreach (library "dialogflow_es" "iam" "monitoring" "speech" "texttospeech"
                 "trace")
    find_package(google_cloud_cpp_${library})
endforeach ()

set(backwards_compat_proto_libraries
    # cmake-format: sortable
    "cloud_dialogflow_v2_protos"
    "cloud_speech_protos"
    "cloud_texttospeech_protos"
    "devtools_cloudtrace_v2_trace_protos"
    "devtools_cloudtrace_v2_tracing_protos"
    "iam_v1_iam_policy_protos"
    "iam_v1_options_protos"
    "iam_v1_policy_protos"
    "logging_type_protos"
    "logging_type_type_protos")

foreach (library ${backwards_compat_proto_libraries})
    add_test_case(test_cmake_${library} google-cloud-cpp::common
                  google-cloud-cpp::${library})
endforeach ()

foreach (library ${backwards_compat_proto_libraries})
    message("library=${library}")
    pkg_check_modules(${library} IMPORTED_TARGET REQUIRED
                      google_cloud_cpp_${library})
    add_test_case(test_pc_${library} PkgConfig::common PkgConfig::${library})
endforeach ()
