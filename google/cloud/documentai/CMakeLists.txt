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

include(GoogleCloudCppLibrary)

google_cloud_cpp_add_gapic_library(documentai "Cloud Document AI API"
                                   SERVICE_DIRS "__EMPTY__" "v1/")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(documentai_quickstart "quickstart/quickstart.cc")
    target_link_libraries(documentai_quickstart
                          PRIVATE google-cloud-cpp::documentai)
    google_cloud_cpp_add_common_options(documentai_quickstart)
    add_test(
        NAME documentai_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:documentai_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_DOCUMENTAI_TEST_LOCATION_ID
            GOOGLE_CLOUD_CPP_DOCUMENTAI_TEST_PROCESSOR_ID
            GOOGLE_CLOUD_CPP_DOCUMENTAI_TEST_FILENAME)
    set_tests_properties(documentai_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
