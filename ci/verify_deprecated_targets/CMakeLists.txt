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

cmake_minimum_required(VERSION 3.6)
project(verify-exported-targets CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMake can use pkg-config to find dependencies. We recommend using CMake
# targets, but we want to verify our pkg-config files remain usable and
# backwards compatible.
find_package(PkgConfig REQUIRED)

find_package(Threads REQUIRED)
# TODO(#5726) - DO NOT CHANGE THESE PACKAGE NAMES These package names are
# expected to remain usable until we have retired them.  This is a test for
# backwards compatibility. If you need to change the test, there is good chance
# you are breaking something.
find_package(bigtable_client REQUIRED)
find_package(firestore_client REQUIRED)
find_package(storage_client REQUIRED)
find_package(spanner_client REQUIRED)
find_package(pubsub_client REQUIRED)

include(CTest)

function (add_test_case name)
    add_executable("${name}" verify_deprecated_targets.cc)
    target_link_libraries("${name}" PRIVATE ${ARGN})
    add_test(NAME "${name}" COMMAND "${name}")
endfunction ()

function (add_proto_test_case name)
    add_executable("${name}" verify_deprecated_proto_targets.cc)
    target_link_libraries("${name}" PRIVATE ${ARGN})
    add_test(NAME "${name}" COMMAND "${name}")
endfunction ()

# TODO(#5726) - DO NOT CHANGE THESE TARGET NAMES These package names are
# expected to remain usable until we have retired them.  This is a test for
# backwards compatibility. If you need to change the test, there is good chance
# you are breaking something.
add_test_case(t010 google_cloud_cpp_common)
add_test_case(t020 google_cloud_cpp_grpc_utils)
add_test_case(t030 bigtable_client)
add_test_case(t040 bigtable::client)
add_test_case(t050 bigtable_protos google_cloud_cpp_common)
add_test_case(t060 bigtable::protos google_cloud_cpp_common)
add_test_case(t070 firestore_client google_cloud_cpp_common)
add_test_case(t080 firestore::client google_cloud_cpp_common)
add_test_case(t090 googleapis-c++::pubsub_client)
add_test_case(t100 pubsub_client)
add_test_case(t120 storage_client)
add_test_case(t130 googleapis-c++::spanner_client)
add_test_case(t140 spanner_client)

# TODO(#5726) - DO NOT CHANGE THESE MODULE NAMES These module names are expected
# to remain usable until we have retired them.  This is a test for backwards
# compatibility. If you need to change the test, there is good chance you are
# breaking something.
set(pkg_config_modules
    google_cloud_cpp_common google_cloud_cpp_grpc_utils bigtable_client
    pubsub_client spanner_client storage_client)
foreach (module ${pkg_config_modules})
    pkg_check_modules(${module} IMPORTED_TARGET REQUIRED ${module})
    add_test_case(test_pc_${module} PkgConfig::${module})
endforeach ()

# This one is special because it does not depend on google_cloud_cpp_common.
pkg_check_modules(firestore_client IMPORTED_TARGET REQUIRED firestore_client)
add_test_case(test_pc_firestore_client PkgConfig::firestore_client
              PkgConfig::google_cloud_cpp_common)

# TODO(#5726) - DO NOT CHANGE THESE TARGET NAMES These target names are expected
# to remain usable until we have retired them.  This is a test for backwards
# compatibility. If you need to change the test, there is good chance you are
# breaking something.
set(googleapis_cpp_installed_libraries_list
    googleapis_cpp_bigtable_protos
    googleapis_cpp_cloud_bigquery_protos
    googleapis_cpp_cloud_speech_protos
    googleapis_cpp_cloud_texttospeech_protos
    googleapis_cpp_iam_protos
    googleapis_cpp_pubsub_protos
    googleapis_cpp_spanner_protos
    googleapis_cpp_storage_protos
    googleapis_cpp_longrunning_operations_protos
    googleapis_cpp_api_http_protos
    googleapis_cpp_api_annotations_protos
    googleapis_cpp_api_auth_protos
    googleapis_cpp_api_client_protos
    googleapis_cpp_api_distribution_protos
    googleapis_cpp_api_field_behavior_protos
    googleapis_cpp_api_label_protos
    googleapis_cpp_api_launch_stage_protos
    googleapis_cpp_api_metric_protos
    googleapis_cpp_api_monitored_resource_protos
    googleapis_cpp_api_resource_protos
    googleapis_cpp_devtools_cloudtrace_v2_trace_protos
    googleapis_cpp_devtools_cloudtrace_v2_tracing_protos
    googleapis_cpp_logging_type_protos
    googleapis_cpp_logging_protos
    googleapis_cpp_monitoring_protos
    googleapis_cpp_iam_v1_options_protos
    googleapis_cpp_iam_v1_policy_protos
    googleapis_cpp_iam_v1_iam_policy_protos
    googleapis_cpp_rpc_error_details_protos
    googleapis_cpp_rpc_status_protos
    googleapis_cpp_type_calendar_period_protos
    googleapis_cpp_type_color_protos
    googleapis_cpp_type_date_protos
    googleapis_cpp_type_datetime_protos
    googleapis_cpp_type_dayofweek_protos
    googleapis_cpp_type_expr_protos
    googleapis_cpp_type_fraction_protos
    googleapis_cpp_type_interval_protos
    googleapis_cpp_type_latlng_protos
    googleapis_cpp_type_localized_text_protos
    googleapis_cpp_type_money_protos
    googleapis_cpp_type_month_protos
    googleapis_cpp_type_phone_number_protos
    googleapis_cpp_type_postal_address_protos
    googleapis_cpp_type_quaternion_protos
    googleapis_cpp_type_timeofday_protos)

foreach (lib ${googleapis_cpp_installed_libraries_list})
    string(REPLACE "googleapis_cpp_" "googleapis-c++::" target "${lib}")
    add_proto_test_case(test_${lib} ${target})
    pkg_check_modules(${lib} IMPORTED_TARGET REQUIRED ${lib})
    add_proto_test_case(test_pc_${lib} PkgConfig::${lib})
endforeach ()
