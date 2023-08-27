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

set(GOOGLE_CLOUD_CPP_SERVICE_DIRS "__EMPTY__")

google_cloud_cpp_add_ga_grpc_library(
    dialogflow_es "Dialogflow API" BACKWARDS_COMPAT_PROTO_TARGETS
    "cloud_dialogflow_v2_protos")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(dialogflow_es_quickstart "quickstart/quickstart.cc")
    target_link_libraries(dialogflow_es_quickstart
                          PRIVATE google-cloud-cpp::dialogflow_es)
    google_cloud_cpp_add_common_options(dialogflow_es_quickstart)
    add_test(
        NAME dialogflow_es_quickstart
        COMMAND cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:dialogflow_es_quickstart> GOOGLE_CLOUD_PROJECT)
    set_tests_properties(dialogflow_es_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
