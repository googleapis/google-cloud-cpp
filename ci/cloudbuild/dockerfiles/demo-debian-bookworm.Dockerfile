# Copyright 2023 Google LLC
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

FROM debian:bookworm
ARG NCPU=4

## [BEGIN packaging.md]

# Install the minimal development tools.

# ```bash
RUN apt-get update && \
    apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ca-certificates cmake curl git \
        gcc g++ m4 make ninja-build pkg-config tar wget zlib1g-dev
# ```

# Install the development packages for direct `google-cloud-cpp` dependencies:

# ```bash
RUN apt-get update && \
    apt-get --no-install-recommends install -y \
        libabsl-dev \
        libprotobuf-dev protobuf-compiler \
        libgrpc++-dev libgrpc-dev protobuf-compiler-grpc \
        libcurl4-openssl-dev libssl-dev nlohmann-json3-dev
# ```

# #### Patching pkg-config

# If you are not planning to use `pkg-config(1)` you can skip these steps.

# Debian's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
# when handling `.pc` files with lots of `Requires:` deps, which happens with
# Abseil. If you plan to use `pkg-config` with any of the installed artifacts,
# you may want to use a recent version of the standard `pkg-config` binary. If
# not, `dnf install pkgconfig` should work.

# ```bash
WORKDIR /var/tmp/build/pkgconf
RUN curl -fsSL https://distfiles.ariadne.space/pkgconf/pkgconf-2.2.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr --with-system-libdir=/lib:/usr/lib --with-system-includedir=/usr/include && \
    make -j ${NCPU:-4} && \
    make install && \
    ldconfig && cd /var/tmp && rm -fr build
ENV PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig
# ```

# #### crc32c

# The project depends on the Crc32c library, we need to compile this from
# source:

# ```bash
WORKDIR /var/tmp/build/crc32c
RUN curl -fsSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
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
