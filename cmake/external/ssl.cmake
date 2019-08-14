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

if (NOT TARGET ssl_project)
    # For OpenSSL we don't really support external projects. OpenSSL build
    # system is notoriously finicky. Use vcpkg on Windows, or install OpenSSL
    # using your operating system packages instead.
    #
    # This file is here to simplify the definition of external projects, such as
    # curl and gRPC, that depend on a SSL library.
    add_custom_target(ssl_project)
    find_package(OpenSSL REQUIRED)
endif ()
