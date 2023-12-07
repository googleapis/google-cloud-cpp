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

google_cloud_cpp_add_gapic_library(
    policytroubleshooter "Policy Troubleshooter API"
    SERVICE_DIRS "__EMPTY__" "v1/" "iam/v3/"
    SHARED_PROTO_DEPS "iam_v2")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(policytroubleshooter_quickstart "quickstart/quickstart.cc")
    target_link_libraries(policytroubleshooter_quickstart
                          PRIVATE google-cloud-cpp::policytroubleshooter)
    google_cloud_cpp_add_common_options(policytroubleshooter_quickstart)
    add_test(
        NAME policytroubleshooter_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:policytroubleshooter_quickstart>
            GOOGLE_CLOUD_CPP_POLICYTROUBLESHOOTER_PRINCIPAL
            GOOGLE_CLOUD_CPP_POLICYTROUBLESHOOTER_RESOURCE
            GOOGLE_CLOUD_CPP_POLICYTROUBLESHOOTER_PERMISSION)
    set_tests_properties(
        policytroubleshooter_quickstart
        PROPERTIES
            LABELS "integration-test;quickstart" ENVIRONMENT
            GOOGLE_CLOUD_CPP_USER_PROJECT=$ENV{GOOGLE_CLOUD_CPP_USER_PROJECT})
endif ()
