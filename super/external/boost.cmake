# ~~~
# Copyright 2020 Google LLC
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

include(ExternalProjectHelper)

if (NOT TARGET boost-project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_BOOST_URL
        "https://dl.bintray.com/boostorg/release/1.71.0/source/boost_1_71_0.tar.gz"
    )
    set(GOOGLE_CLOUD_CPP_BOOST_SHA256
        "96b34f7468f26a141f6020efb813f1a2f3dfb9797ecf76a7d7cbd843cc95f5bd")

    set_external_project_vars()
    ExternalProject_Add(
        boost-project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/boost"
        INSTALL_DIR "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
        URL ${GOOGLE_CLOUD_CPP_BOOST_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_BOOST_SHA256}
                 LIST_SEPARATOR
                 |
        CONFIGURE_COMMAND ./bootstrap.sh --prefix=<INSTALL_DIR>
        BUILD_COMMAND ./b2 install BUILD_IN_SOURCE ON
        INSTALL_COMMAND ""
        LOG_DOWNLOAD ON
        LOG_CONFIGURE OFF
        LOG_BUILD OFF
        LOG_INSTALL OFF)
endif ()
