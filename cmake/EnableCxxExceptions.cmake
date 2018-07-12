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

option(GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS "Enable C++ Exception Support" ON)

# If the user disabled C++ exceptions we should give them a heads up about the
# consequences.
if (NOT GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    message(
        WARNING
            "C++ Exceptions disabled, any operation that normally"
            " raises exceptions will instead log the error and call"
            " std::abort().  In addition, some examples and tests will not be"
            " compiled.")
    if (MSVC)
        set(GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG "/EHs-c-")
    else()
        set(GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG "-fno-exceptions")
    endif ()
else()
    set(GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG "")
endif ()
