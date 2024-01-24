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

if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    set(universe_domain_demos bigquery compute kms pubsub storage)
    foreach (library ${universe_domain_demos})
        if (${library} IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
            add_executable(universe_domain_${library}_demo "demo/${library}.cc")
            target_link_libraries(
                universe_domain_${library}_demo
                PRIVATE google-cloud-cpp::${library}
                        google-cloud-cpp::universe_domain)
            google_cloud_cpp_add_common_options(universe_domain_${library}_demo)
        endif ()
    endforeach ()

    if (bigquery IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        add_test(
            NAME universe_domain_bigquery_demo
            COMMAND
                cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:universe_domain_bigquery_demo>
                GOOGLE_CLOUD_PROJECT
                GOOGLE_CLOUD_CPP_BIGQUERY_TEST_QUICKSTART_TABLE)
        set_tests_properties(universe_domain_bigquery_demo
                             PROPERTIES LABELS "integration-test;quickstart")
    endif ()

    if (compute IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        add_test(
            NAME universe_domain_compute_demo
            COMMAND
                cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:universe_domain_compute_demo>
                GOOGLE_CLOUD_PROJECT GOOGLE_CLOUD_CPP_TEST_ZONE)
        set_tests_properties(universe_domain_compute_demo
                             PROPERTIES LABELS "integration-test;quickstart")
    endif ()

    if (kms IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        add_test(
            NAME universe_domain_kms_demo
            COMMAND
                cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:universe_domain_kms_demo> GOOGLE_CLOUD_PROJECT
                GOOGLE_CLOUD_CPP_TEST_REGION)
        set_tests_properties(universe_domain_kms_demo
                             PROPERTIES LABELS "integration-test;quickstart")
    endif ()

    if (pubsub IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        add_test(
            NAME universe_domain_pubsub_demo
            COMMAND
                cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:universe_domain_pubsub_demo> GOOGLE_CLOUD_PROJECT)
        set_tests_properties(universe_domain_pubsub_demo
                             PROPERTIES LABELS "integration-test;quickstart")
    endif ()

    if (storage IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        add_test(
            NAME universe_domain_storage_demo
            COMMAND
                cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:universe_domain_storage_demo>
                GOOGLE_CLOUD_PROJECT)
        set_tests_properties(universe_domain_storage_demo
                             PROPERTIES LABELS "integration-test;quickstart")
    endif ()
endif ()
