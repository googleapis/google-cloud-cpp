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

WORKDIR /var/tmp/build
RUN curl -s -L -o 1.1.0.tar.gz https://github.com/google/crc32c/archive/1.1.0.tar.gz
RUN tar -zxf 1.1.0.tar.gz
WORKDIR /var/tmp/build/crc32c-1.1.0
RUN cmake -S. -B.build -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCRC32C_BUILD_TESTS=OFF \
    -DCRC32C_BUILD_BENCHMARKS=OFF \
    -DCRC32C_USE_GLOG=OFF
RUN cmake --build .build --target install

WORKDIR /var/tmp/build
RUN curl -s -L -o v0.2.1.tar.gz https://github.com/googleapis/cpp-cmakefiles/archive/v0.2.1.tar.gz
RUN tar -zxf v0.2.1.tar.gz
WORKDIR /var/tmp/build/cpp-cmakefiles-0.2.1
RUN cmake -S. -B.build -GNinja \
    -DCMAKE_BUILD_TYPE=Release
RUN cmake --build .build --target install

WORKDIR /var/tmp/build
RUN curl -s -L -o v0.17.0.tar.gz \
    https://github.com/googleapis/google-cloud-cpp-common/archive/v0.17.0.tar.gz
RUN tar -zxf v0.17.0.tar.gz
WORKDIR /var/tmp/build/google-cloud-cpp-common-0.17.0
RUN cmake -S. -B.build -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF
RUN cmake --build .build --target install

WORKDIR /var/tmp/build
RUN curl -s -L -o v0.17.0.tar.gz \
    https://github.com/googleapis/google-cloud-cpp/archive/v0.17.0.tar.gz
RUN tar -zxf v0.17.0.tar.gz
WORKDIR /var/tmp/build/google-cloud-cpp-0.17.0
RUN cmake -S. -B.build -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF
RUN cmake --build .build --target install

WORKDIR /var/tmp/build
RUN curl -s -L -o v0.5.0.tar.gz \
    https://github.com/googleapis/google-cloud-cpp-spanner/archive/v0.5.0.tar.gz
RUN tar -zxf v0.5.0.tar.gz
WORKDIR /var/tmp/build/google-cloud-cpp-spanner-0.5.0
RUN cmake -S. -B.build -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF
RUN cmake --build .build --target install

RUN apk update && \
    apk add \
        c-ares-static

FROM devtools AS build
COPY . /v/source
WORKDIR /v/source
RUN cmake -S/v/source -B/v/binary -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBoost_USE_STATIC_LIBS=ON \
    -D_gRPC_USE_STATIC_LIBS=ON \
    -DProtobuf_USE_STATIC_LIBS=ON \
    -DOPENSSL_USE_STATIC_LIBS=ON \
    -DOPENSSL_CRYPTO_LIBRARY=/usr/lib/libcrypto.a \
    -DOPENSSL_SSL_LIBRARY=/usr/lib/libssl.a \
    -DZLIB_LIBRARY=/lib/libz.a \
    -DEXTRA_LIBRARY="/usr/lib/libnghttp2.a;/usr/lib/libcares.a" \
    -DCMAKE_EXE_LINKER_FLAGS="-static"

RUN cmake --build /v/binary
RUN strip /v/binary/pubsub_handler

FROM base AS with-certs
RUN apk update && \
    apk add ca-certificates && \
    rm -rf /var/cache/apk/*
RUN update-ca-certificates

FROM with-certs AS pubsub-handler
WORKDIR /r
COPY --from=build /v/binary/pubsub_handler /r

CMD "/r/pubsub_handler" \
    "--project=${SPANNER_PROJECT}" \
    "--instance=${SPANNER_INSTANCE}" \
    "--database=${SPANNER_DATABASE}"

FROM with-certs AS tools
WORKDIR /r
COPY --from=build /v/binary/create_database /r
COPY --from=build /v/binary/generate_randomly_named_objects /r
COPY --from=build /v/binary/refresh_bucket /r

CMD "/bin/false"
