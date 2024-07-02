# ~~~
# Copyright 2023 Google LLC
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
    bigquery "Google Cloud BigQuery API"
    SERVICE_DIRS
        "__EMPTY__"
        "analyticshub/v1/"
        "biglake/v1/"
        "connection/v1/"
        "datapolicies/v1/"
        "datatransfer/v1/"
        "migration/v2/"
        "reservation/v1/"
        "storage/v1/"
    BACKWARDS_COMPAT_PROTO_TARGETS "cloud_bigquery_protos"
                                   ADDITIONAL_DOXYGEN_DIRS "job/v2/")

# Examples are enabled if possible, but package maintainers may want to disable
# compilation to speed up their builds.
if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)
    add_executable(bigquery_quickstart "quickstart/quickstart.cc")
    target_link_libraries(bigquery_quickstart
                          PRIVATE google-cloud-cpp::bigquery)
    google_cloud_cpp_add_common_options(bigquery_quickstart)
    add_test(
        NAME bigquery_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:bigquery_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_BIGQUERY_TEST_QUICKSTART_TABLE)
    set_tests_properties(bigquery_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()

# BigQuery has a handwritten sample that demonstrates mocking. The executable is
# added by `google_cloud_cpp_add_gapic_library()`. We need to manually link it
# against Google Mock.
if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    target_link_libraries(bigquery_samples_mock_bigquery_read
                          PRIVATE GTest::gmock_main)
endif ()
