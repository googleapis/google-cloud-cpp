# Copyright 2022 Google LLC
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

FROM alpine:3.16
ARG NCPU=4

## [BEGIN packaging.md]

# Install the minimal development tools, libcurl, and OpenSSL:

# ```bash
RUN apk update && \
    apk add bash ca-certificates cmake curl git \
        gcc g++ make tar unzip zip zlib-dev
# ```

# Alpine's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
# when handling `.pc` files with lots of `Requires:` deps, which happens with
# Abseil, so we use the normal `pkg-config` binary, which seems to not suffer
# from this bottleneck. For more details see
# https://github.com/pkgconf/pkgconf/issues/229 and
# https://github.com/googleapis/google-cloud-cpp/issues/7052.

# ```bash
WORKDIR /var/tmp/build/pkg-config-cpp
RUN curl -fsSL https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --with-internal-glib && \
    make -j ${NCPU:-4} && \
    make install
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig
# ```

# #### Dependencies

# The versions of Abseil, Protobuf, gRPC, OpenSSL, and nlohmann-json included
# with Alpine >= 3.16 meet `google-cloud-cpp`'s requirements. We can simply
# install the development packages

# ```bash
RUN apk update && \
    apk add abseil-cpp-dev c-ares-dev curl-dev grpc-dev \
        protobuf-dev nlohmann-json openssl-dev re2-dev
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
    cmake --build cmake-out --target install -- -j ${NCPU:-4}
# ```

# #### opentelemetry-cpp

# The project has an **optional** dependency on the OpenTelemetry library.
# We recommend installing this library because:
# - the dependency will become required in the google-cloud-cpp v3.x series.
# - it is needed to produce distributed traces of the library.

# ```bash
WORKDIR /var/tmp/build/opentelemetry-cpp
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.13.0.tar.gz | \
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
    cmake --build cmake-out --target install -- -j ${NCPU:-4}
# ```

## [DONE packaging.md]

WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.7.7/sccache-v0.7.7-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache
