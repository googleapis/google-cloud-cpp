#!/usr/bin/env bash
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

source "${DESTINATION_ROOT}/ci/etc/quickstart-config.sh"

declare -A ORIGINAL_COPYRIGHT_YEAR=(
  ["centos-7"]=2018
  ["centos-8"]=2019
  ["debian-buster"]=2019
  ["debian-stretch"]=2018
  ["fedora"]=2018
  ["opensuse-leap"]=2019
  ["opensuse-tumbleweed"]=2018
  ["ubuntu-xenial"]=2018
  ["ubuntu-bionic"]=2018
  ["ubuntu-focal"]=2020
)

declare -a FROZEN_FILES=()

read -r -d '' QUICKSTART_FRAGMENT < <(
  for library in $(quickstart_libraries); do
    tr '\n' '\003' <<_EOF_
WORKDIR /home/build/quickstart-cmake/${library}
COPY google/cloud/${library}/quickstart /home/build/quickstart-cmake/${library}
RUN env -u PKG_CONFIG_PATH cmake -H. -B/i/${library}
RUN cmake --build /i/${library}
_EOF_
  done
) || true # read always exit with error

BUILD_AND_TEST_PROJECT_FRAGMENT=$(
  replace_fragments \
    "INSTALL_CRC32C_FROM_SOURCE" \
    "INSTALL_CPP_CMAKEFILES_FROM_SOURCE" \
    "INSTALL_GOOGLETEST_FROM_SOURCE" \
    "INSTALL_GOOGLE_CLOUD_CPP_COMMON_FROM_SOURCE" \
    "QUICKSTART_FRAGMENT" <<'_EOF_'
# #### crc32c

# The project depends on the Crc32c library, we need to compile this from
# source:

# ```bash
@INSTALL_CRC32C_FROM_SOURCE@
# ```

# #### googleapis

# We need a recent version of the Google Cloud Platform proto C++ libraries:

# ```bash
@INSTALL_CPP_CMAKEFILES_FROM_SOURCE@
# ```

# #### googletest

# We need a recent version of GoogleTest to compile the unit and integration
# tests.

# ```bash
@INSTALL_GOOGLETEST_FROM_SOURCE@
# ```

FROM devtools AS install
ARG NCPU=4
ARG DISTRO="distro-name"

# #### Compile and install the main project

# We can now compile, test, and install `@GOOGLE_CLOUD_CPP_REPOSITORY@`.

# ```bash
WORKDIR /home/build/project
COPY . /home/build/project
## [START IGNORED]
# This is just here to speed up the pre-submit builds and should not be part
# of the instructions on how to compile the code.
ENV CCACHE_DIR=/h/.ccache
RUN mkdir -p /h/.ccache; \
    echo "max_size = 4.0G" >"/h/.ccache/ccache.conf"; \
    if [ -r "ci/kokoro/install/ccache-contents/${DISTRO}.tar.gz" ]; then \
      tar -xf "ci/kokoro/install/ccache-contents/${DISTRO}.tar.gz" -C /h; \
      ccache --show-stats; \
      ccache --zero-stats; \
    fi; \
    true # Ignore all errors, failures in caching should not break the build
## [END IGNORED]
RUN cmake -H. -Bcmake-out
RUN cmake --build cmake-out -- -j "${NCPU:-4}"
WORKDIR /home/build/project/cmake-out
RUN ctest -LE integration-tests --output-on-failure
RUN cmake --build . --target install
# ```

## [END INSTALL.md]

ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig

# Verify that the installed files are actually usable
WORKDIR /home/build/bigtable-make
COPY google/cloud/bigtable/quickstart /home/build/bigtable-make
RUN make

WORKDIR /home/build/storage-make
COPY google/cloud/storage/quickstart /home/build/storage-make
RUN make

@QUICKSTART_FRAGMENT@

# This is just here to speed up the pre-submit builds and should not be part
# of the instructions on how to compile the code.
RUN ccache --show-stats --zero-stats || true
_EOF_
)
