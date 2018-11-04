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

add_executable(bigtable_hello_world bigtable_hello_world.cc)
target_link_libraries(bigtable_hello_world bigtable_client)

add_executable(bigtable_hello_app_profile bigtable_hello_app_profile.cc)
target_link_libraries(bigtable_hello_app_profile bigtable_client)

add_executable(bigtable_quickstart bigtable_quickstart.cc)
target_link_libraries(bigtable_quickstart bigtable_client)

add_executable(bigtable_samples bigtable_samples.cc)
target_link_libraries(bigtable_samples bigtable_client)

add_executable(bigtable_samples_instance_admin
               bigtable_samples_instance_admin.cc)
target_link_libraries(bigtable_samples_instance_admin bigtable_client)

add_executable(bigtable_samples_instance_admin_ext
               bigtable_samples_instance_admin_ext.cc)
target_link_libraries(bigtable_samples_instance_admin_ext bigtable_client)

add_executable(table_admin_async_snippets table_admin_async_snippets.cc)
target_link_libraries(table_admin_async_snippets bigtable_client)

add_executable(table_admin_snippets table_admin_snippets.cc)
target_link_libraries(table_admin_snippets bigtable_client)

add_executable(data_snippets data_snippets.cc)
target_link_libraries(data_snippets bigtable_client)
