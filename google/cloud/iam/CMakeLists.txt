# ~~~
# Copyright 2020 Google LLC
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
    iam "Google Cloud IAM API"
    SERVICE_DIRS "__EMPTY__" "admin/v1/" "credentials/v1/" "v1/" "v2/" "v3/"
    SHARED_PROTO_DEPS "iam_v2" "iam_v3")

# Examples are enabled if possible, but package maintainers may want to disable
# compilation to speed up their builds.
if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)
    add_executable(iam_quickstart "quickstart/quickstart.cc")
    target_link_libraries(iam_quickstart PRIVATE google-cloud-cpp::iam)
    google_cloud_cpp_add_common_options(iam_quickstart)
    add_test(
        NAME iam_quickstart
        COMMAND cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:iam_quickstart> GOOGLE_CLOUD_PROJECT)
    set_tests_properties(iam_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()

# IAM has some handwritten samples that demonstrate mocking. The executables are
# added by `google_cloud_cpp_add_gapic_library()`. We need to manually link them
# against Google Mock.
if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    target_link_libraries(iam_samples_mock_iam PRIVATE GTest::gmock_main)
    target_link_libraries(iam_samples_mock_iam_credentials
                          PRIVATE GTest::gmock_main)
endif ()
