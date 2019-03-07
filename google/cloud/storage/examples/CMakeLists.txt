# ~~~
# Copyright 2018 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

# While it is tempting create these targets using a function or a loop, we want
# to keep this particular file as simple as possible, as it is intended to be
# part of the examples.

add_executable(storage_bucket_samples storage_bucket_samples.cc)
target_link_libraries(storage_bucket_samples storage_client
                      storage_common_options)

add_executable(storage_bucket_acl_samples storage_bucket_acl_samples.cc)
target_link_libraries(storage_bucket_acl_samples storage_client
                      storage_common_options)

add_executable(storage_bucket_iam_samples storage_bucket_iam_samples.cc)
target_link_libraries(storage_bucket_iam_samples storage_client
                      storage_common_options)

add_executable(storage_default_object_acl_samples
               storage_default_object_acl_samples.cc)
target_link_libraries(storage_default_object_acl_samples storage_client
                      storage_common_options)

add_executable(storage_object_samples storage_object_samples.cc)
target_link_libraries(storage_object_samples storage_client
                      storage_common_options)

add_executable(storage_object_acl_samples storage_object_acl_samples.cc)
target_link_libraries(storage_object_acl_samples storage_client
                      storage_common_options)

add_executable(storage_notification_samples storage_notification_samples.cc)
target_link_libraries(storage_notification_samples storage_client
                      storage_common_options)

add_executable(storage_quickstart storage_quickstart.cc)
target_link_libraries(storage_quickstart
                      storage_client
                      storage_common_options
                      google_cloud_cpp_common_options)

add_executable(storage_service_account_samples
               storage_service_account_samples.cc)
target_link_libraries(storage_service_account_samples storage_client
                      storage_common_options)
