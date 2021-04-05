# ~~~
# Copyright 2019 Google LLC
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

# When compiling against *-static vcpkg packages we need to use the static C++
# runtime with MSVC. The default is to use the dynamic runtime, which does not
# work in this case.  This seems to be the recommended way to change the
# runtime:
#
# ~~~
#  https://gitlab.kitware.com/cmake/community/wikis/FAQ#how-can-i-build-my-msvc-application-with-a-static-runtime
# ~~~
#
# Note that currently we use VCPKG_TARGET_TRIPLET to determine if the static
# runtime is needed. In the future we may need to use a more complex expression
# to determine this, but this is a good start.
#

if (MSVC)
    if (VCPKG_TARGET_TRIPLET MATCHES "-static$")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else ()
        set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif ()
endif ()
