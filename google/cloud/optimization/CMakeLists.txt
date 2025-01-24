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

google_cloud_cpp_add_gapic_library(optimization "Cloud Optimization API"
                                   SERVICE_DIRS "__EMPTY__" "v1/")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(optimization_quickstart "quickstart/quickstart.cc")
    target_link_libraries(optimization_quickstart
                          PRIVATE google-cloud-cpp::optimization)
    google_cloud_cpp_add_common_options(optimization_quickstart)
    add_test(
        NAME optimization_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:optimization_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME)
    set_tests_properties(
        optimization_quickstart
        PROPERTIES
            LABELS
            "integration-test;quickstart"
            PASS_REGULAR_EXPRESSION
            "The service is currently unavailable.*gcloud-cpp.retry.function=BatchOptimizeTours"
    )
endif ()
