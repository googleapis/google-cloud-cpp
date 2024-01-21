# ~~~
# Copyright 2024 Google LLC
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

if (BUILD_UNIVERSE_DOMAIN_DEMOS AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(universe_domain_kms_demo "demo/kms.cc")
    target_link_libraries(
        universe_domain_kms_demo PRIVATE google-cloud-cpp::kms
                                         google-cloud-cpp::universe_domain)
    google_cloud_cpp_add_common_options(universe_domain_kms_demo)
    add_test(
        NAME universe_domain_kms_demo
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:universe_domain_kms_demo> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_TEST_REGION)
    set_tests_properties(universe_domain_kms_demo
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
