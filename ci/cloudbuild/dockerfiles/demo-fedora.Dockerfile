# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

FROM fedora:37
ARG NCPU=4

## [BEGIN packaging.md]

# Install the minimal development tools:

# ```bash
RUN dnf makecache && \
    dnf install -y cmake curl findutils gcc-c++ git make ninja-build \
        openssl-devel patch unzip tar wget zip zlib-devel
# ```

# Fedora 37 includes packages, with recent enough versions, for most of the
# direct dependencies of `google-cloud-cpp`.

# ```bash
RUN dnf makecache && \
    dnf install -y protobuf-compiler protobuf-devel grpc-cpp grpc-devel \
        libcurl-devel google-crc32c-devel
# ```

# #### Patching pkg-config

# If you are not planning to use `pkg-config(1)` you can skip these steps.

# Fedora's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
# when handling `.pc` files with lots of `Requires:` deps, which happens with
# Abseil. If you plan to use `pkg-config` with any of the installed artifacts,
# you may want to use a recent version of the standard `pkg-config` binary. If
# not, `dnf install pkgconfig` should work.

# ```bash
WORKDIR /var/tmp/build/pkg-config-cpp
RUN curl -fsSL https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --with-internal-glib && \
    make -j ${NCPU:-4} && \
    make install && \
    ldconfig
# ```

# The version of RE2 included with this distro hard-codes C++11 in its
# pkg-config file. This is fixed in later versions, and it is unnecessary as
# Fedora's compiler defaults to C++17.  If you are not planning to use
# `pkg-config(1)` you can ignore this step.  Alternatively, you can install
# RE2 and gRPC from source.

# ```bash
WORKDIR /var/tmp/build/re2
RUN sed -i 's/-std=c\+\+11 //' /usr/lib64/pkgconfig/re2.pc
# ```

# The following steps will install libraries and tools in `/usr/local`. By
# default, pkg-config does not search in these directories.

# ```bash
ENV PKG_CONFIG_PATH=/usr/local/share/pkgconfig:/usr/lib64/pkgconfig
# ```

# #### nlohmann_json library

# The project depends on the nlohmann_json library. We use CMake to
# install it as this installs the necessary CMake configuration files.
# Note that this is a header-only library, and often installed manually.
# This leaves your environment without support for CMake pkg-config.

# ```bash
WORKDIR /var/tmp/build/json
RUN curl -fsSL https://github.com/nlohmann/json/archive/v3.11.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -S . -B cmake-out && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

## [DONE packaging.md]

WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.5.4/sccache-v0.5.4-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Update the ld.conf cache in case any libraries installed in /usr/local/lib*
RUN ldconfig /usr/local/lib*
