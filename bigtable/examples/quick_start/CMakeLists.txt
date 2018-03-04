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

# While it is tempting create these targets using a function or a loop, we want
# to keep this particular file as simple as possible, as it is intended to be
# part of the examples.

add_executable(bigtable_create_table bigtable_create_table.cc)
target_link_libraries(bigtable_create_table bigtable_client)

add_executable(bigtable_write_row bigtable_write_row.cc)
target_link_libraries(bigtable_write_row bigtable_client)

add_executable(bigtable_read_row bigtable_read_row.cc)
target_link_libraries(bigtable_read_row bigtable_client)
