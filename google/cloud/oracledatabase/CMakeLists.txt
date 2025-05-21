# ~~~
# Copyright 2025 Google LLC
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

include(GoogleCloudCppLibrary)

google_cloud_cpp_add_gapic_library(
    oracledatabase "Oracle Database@Google Cloud API" SERVICE_DIRS "v1/")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(oracledatabase_quickstart "quickstart/quickstart.cc")
    target_link_libraries(oracledatabase_quickstart
                          PRIVATE google-cloud-cpp::oracledatabase)
    google_cloud_cpp_add_common_options(oracledatabase_quickstart)
    add_test(
        NAME oracledatabase_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:oracledatabase_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_US_EAST_TEST_REGION)
    set_tests_properties(oracledatabase_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
