# Copyright 2025 Google LLC
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

FROM alpine:3.20 AS build

# Install the minimal development tools:
RUN apk update && \
    apk add bash ca-certificates cmake curl git \
        gcc g++ make ninja-build ninja-is-really-ninja patch tar unzip zip zlib-dev

# The versions of Abseil (>= 20230802.1), Protobuf (>= v24.4), gRPC (>= 1.59.3),
# OpenSSL, and nlohmann-json included with Alpine are good enough for this
# benchmark.
RUN apk update && \
    apk add abseil-cpp-dev crc32c-dev c-ares-dev curl-dev grpc-dev \
        protobuf-dev nlohmann-json openssl-dev re2-dev

# Alpine does not include a package for OpenTelemetry, so we compile it from
# source.
WORKDIR /var/tmp/build/opentelemetry-cpp
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.16.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -G Ninja -S . -B .build \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_EXAMPLES=OFF \
        -DWITH_ABSEIL=ON \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 && \
    cmake --build .build && \
    cmake --install .build && \
    rm -fr .build

WORKDIR /var/tmp/build/google-cloud-cpp
COPY . /var/tmp/build/google-cloud-cpp/
RUN cmake \
      -G Ninja -S . -B .build \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DGOOGLE_CLOUD_CPP_WITH_MOCKS=OFF \
      -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
      -DGOOGLE_CLOUD_CPP_ENABLE=storage,storage_grpc,monitoring,opentelemetry && \
    cmake --build .build && \
    cmake --install .build && \
    rm -fr .build

RUN apk update && \
    apk add boost-dev

# Update the ld.conf cache in case any libraries were installed in /usr/local/lib*
RUN ldconfig /usr/local/lib*
