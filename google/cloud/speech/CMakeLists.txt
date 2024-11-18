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
    speech "Cloud Speech-to-Text API"
    SERVICE_DIRS "__EMPTY__" "v1/" "v2/"
    BACKWARDS_COMPAT_PROTO_TARGETS "cloud_speech_protos")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(speech_quickstart "quickstart/quickstart.cc")
    target_link_libraries(speech_quickstart PRIVATE google-cloud-cpp::speech)
    google_cloud_cpp_add_common_options(speech_quickstart)
    add_test(
        NAME speech_quickstart_global
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:speech_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_TEST_GLOBAL)
    set_tests_properties(speech_quickstart_global
                         PROPERTIES LABELS "integration-test;quickstart")
    add_test(
        NAME speech_quickstart_us_regional
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:speech_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_TEST_REGION)
    set_tests_properties(speech_quickstart_us_regional
                         PROPERTIES LABELS "integration-test;quickstart")
    add_test(
        NAME speech_quickstart_non_us_regional
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:speech_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_NON_US_TEST_REGION)
    set_tests_properties(speech_quickstart_non_us_regional
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
