# ~~~
# Copyright 2018 Google LLC
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

# protobuf always requires thread support.
find_package(Threads REQUIRED)

# Configure the protobuf dependency, this can be found as a external project,
# package, or  installed with pkg-config support.
set(GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER ${GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER}
    CACHE STRING "How to find protobuf libraries and compiler")
set_property(CACHE GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER
             PROPERTY STRINGS "external" "package")

if ("${GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER}" STREQUAL "external")
    include(external/protobuf)
elseif("${GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER}" STREQUAL "package")
    find_package(ProtobufTargets REQUIRED protobuf>=3.5)
endif ()
