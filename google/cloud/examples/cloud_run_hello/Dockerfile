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

ARG DISTRO_VERSION=edge
FROM alpine:${DISTRO_VERSION} AS base

FROM base AS devtools

# Install the typical development tools and some additions:
#   - ninja-build is a backend for CMake that often compiles faster than
#     CMake with GNU Make.
#   - Install the boost libraries.
RUN apk update && \
    apk add \
        boost-dev \
        boost-static \
        build-base \
        c-ares-static \
        cmake \
        curl \
        curl-dev \
        curl-static \
        git \
        gcc \
        g++ \
        grpc \
        grpc-dev \
        grpc-cli \
        libc-dev \
        nghttp2-static \
        ninja \
        protobuf \
        protobuf-dev \
        openssl-dev \
        openssl-libs-static \
        tar \
        zlib-static

FROM devtools AS build
COPY . /v/source
WORKDIR /v/source
RUN cmake -S/v/source -B/v/binary -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBoost_USE_STATIC_LIBS=ON \
    -DEXTRA_LIBRARY="/usr/lib/libnghttp2.a;/usr/lib/libcares.a" \
    -DCMAKE_EXE_LINKER_FLAGS="-static"

RUN cmake --build /v/binary
RUN strip /v/binary/cloud_run_hello

FROM base AS with-certs
RUN apk update && \
    apk add ca-certificates && \
    rm -rf /var/cache/apk/*
RUN update-ca-certificates

FROM with-certs AS cloud-run-hello
WORKDIR /r
COPY --from=build /v/binary/cloud_run_hello /r

CMD "/r/cloud_run_hello"
