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

if (APPLE AND (Protobuf_VERSION VERSION_LESS 23.0))
    message(
        WARNING "Cannot build google/cloud/storagetransfer - "
                " on macOS GID_MAX is a system macro and is also used as"
                " a C++ symbol in this service protos. Protobuf < 23.0"
                " does not have the necessary workarounds to handle this"
                " conflict.")
    return()
endif ()

include(GoogleCloudCppLibrary)

google_cloud_cpp_add_gapic_library(storagetransfer "Storage Transfer API"
                                   SERVICE_DIRS "__EMPTY__" "v1/")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(storagetransfer_quickstart "quickstart/quickstart.cc")
    target_link_libraries(storagetransfer_quickstart
                          PRIVATE google-cloud-cpp::storagetransfer)
    google_cloud_cpp_add_common_options(storagetransfer_quickstart)
    add_test(
        NAME storagetransfer_quickstart
        COMMAND cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:storagetransfer_quickstart> GOOGLE_CLOUD_PROJECT)
    set_tests_properties(storagetransfer_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
