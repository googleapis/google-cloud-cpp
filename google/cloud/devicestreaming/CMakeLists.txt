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

google_cloud_cpp_add_gapic_library(devicestreaming "Device Streaming API"
                                   SERVICE_DIRS "v1/")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(devicestreaming_quickstart "quickstart/quickstart.cc")
    target_link_libraries(devicestreaming_quickstart
                          PRIVATE google-cloud-cpp::devicestreaming)
    google_cloud_cpp_add_common_options(devicestreaming_quickstart)
    add_test(
        NAME devicestreaming_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:devicestreaming_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_TEST_REGION # EDIT HERE
    )
    set_tests_properties(devicestreaming_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
