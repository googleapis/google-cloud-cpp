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

FROM fedora:39
ARG NCPU=4

## [BEGIN packaging.md]

# Install the minimal development tools:

# ```bash
RUN dnf makecache && \
    dnf install -y cmake curl findutils gcc-c++ git make ninja-build \
        openssl-devel patch unzip tar wget zip zlib-devel
# ```

# Fedora 38 includes packages, with recent enough versions, for most of the
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
WORKDIR /var/tmp/build/pkgconf
RUN curl -fsSL https://distfiles.ariadne.space/pkgconf/pkgconf-2.2.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr --with-system-libdir=/lib64:/usr/lib64 --with-system-includedir=/usr/include && \
    make -j ${NCPU:-4} && \
    make install && \
    ldconfig && cd /var/tmp && rm -fr build
# ```

# Older versions of Fedora hard-code RE2 to use C++11. It was fixed starting
# with Fedora:38. If you using Fedora >= 38 or you are not planning to use
# `pkg-config(1)` you can ignore this step.  Alternatively, you can install RE2
# and gRPC from source.
# ```
# sed -i 's/-std=c\+\+11 //' /usr/lib64/pkgconfig/re2.pc
# ```

# The following steps will install libraries and tools in `/usr/local`. By
# default, pkgconf does not search in these directories. We need to explicitly
# set the search path.

# ```bash
ENV PKG_CONFIG_PATH=/usr/local/share/pkgconfig:/usr/lib64/pkgconfig:/usr/local/lib64/pkgconfig
# ```

# #### nlohmann_json library

# The project depends on the nlohmann_json library. We use CMake to
# install it as this installs the necessary CMake configuration files.
# Note that this is a header-only library, and often installed manually.
# This leaves your environment without support for CMake pkg-config.

# ```bash
WORKDIR /var/tmp/build/json
RUN curl -fsSL https://github.com/nlohmann/json/archive/v3.11.3.tar.gz | \
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

# #### opentelemetry-cpp

# The project has an **optional** dependency on the OpenTelemetry library.
# We recommend installing this library because:
# - the dependency will become required in the google-cloud-cpp v3.x series.
# - it is needed to produce distributed traces of the library.

# ```bash
WORKDIR /var/tmp/build/opentelemetry-cpp
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.14.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_ABSEIL=ON \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

## [DONE packaging.md]

WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.8.0/sccache-v0.8.0-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Update the ld.conf cache in case any libraries were installed in /usr/local/lib*
RUN ldconfig /usr/local/lib*
