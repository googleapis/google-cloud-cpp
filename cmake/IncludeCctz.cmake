# Copyright 2017 Google Inc.
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

# While the documentation is not clear as to whether cctz can be used as an
# installed library, we only use it as part of abseil, which is supposed to
# be included as a module.

if (NOT CCTZ_ROOT_DIR)
    set(CCTZ_ROOT_DIR ${PROJECT_SOURCE_DIR}/third_party/cctz)
endif ()

if (NOT EXISTS "${CCTZ_ROOT_DIR}/CMakeLists.txt")
    message(ERROR "expected a CMakeLists.txt in CCTZ_ROOT_DIR")
endif ()

# cctz will include the `CTest` module and always compile the cctz tests, we
# want to disable that.  The only way is to include the module first, disable
# the tests, and then include the cctz CMakeLists.txt files.
include(CTest)
set(BUILD_TESTING OFF)

add_subdirectory(${CCTZ_ROOT_DIR} third_party/cctz EXCLUDE_FROM_ALL)
set(CCTZ_LIBRARIES cctz)
set(CCTZ_INCLUDE_DIRS ${CCTZ_ROOT_DIR}/absl)
