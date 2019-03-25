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

# gRPC always requires thread support.
find_package(Threads REQUIRED)

# Configure the gRPC dependency, this can be found as a submodule, package, or
# installed with pkg-config support.
set(GOOGLE_CLOUD_CPP_CRC32C_PROVIDER ${GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER}
    CACHE STRING "How to find the Crc32c library")
set_property(CACHE GOOGLE_CLOUD_CPP_CRC32C_PROVIDER
             PROPERTY STRINGS "external" "package")

if (TARGET Crc32c::crc32c)
    # Crc32c::crc32c is already defined, do not define it again.
elseif("${GOOGLE_CLOUD_CPP_CRC32C_PROVIDER}" STREQUAL "external")
    include(external/crc32c)
elseif("${GOOGLE_CLOUD_CPP_CRC32C_PROVIDER}" STREQUAL "package")
    find_package(Crc32c CONFIG REQUIRED)
endif ()
