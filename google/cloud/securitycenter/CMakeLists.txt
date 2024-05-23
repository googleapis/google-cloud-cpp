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

if (WIN32 AND (Protobuf_VERSION VERSION_LESS 27.0))
    message(
        WARNING "Cannot build google/cloud/securitycenter on Windows"
                " - the protos have enum names that conflict with some system"
                " macros. Update to protobuf >= 27.0.")
    return()
endif ()

include(GoogleCloudCppLibrary)

google_cloud_cpp_add_gapic_library(securitycenter "Security Command Center API"
                                   SERVICE_DIRS "__EMPTY__" "v1/" "v2/")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(securitycenter_quickstart "quickstart/quickstart.cc")
    target_link_libraries(securitycenter_quickstart
                          PRIVATE google-cloud-cpp::securitycenter)
    google_cloud_cpp_add_common_options(securitycenter_quickstart)
    add_test(
        NAME securitycenter_quickstart
        COMMAND cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:securitycenter_quickstart> GOOGLE_CLOUD_PROJECT)
    set_tests_properties(
        securitycenter_quickstart
        PROPERTIES
            LABELS "integration-test;quickstart" ENVIRONMENT
            GOOGLE_CLOUD_CPP_USER_PROJECT=$ENV{GOOGLE_CLOUD_CPP_USER_PROJECT})
endif ()
