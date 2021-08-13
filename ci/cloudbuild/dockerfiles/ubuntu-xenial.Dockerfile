# Copyright 2021 Google LLC
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

FROM ubuntu:16.04
ARG NCPU=4

RUN apt-get update && \
    apt-get install -y \
        automake \
        build-essential \
        ccache \
        clang \
        cmake \
        ctags \
        curl \
        gawk \
        git \
        gcc \
        g++ \
        cmake \
        libcurl4-openssl-dev \
        libssl-dev \
        libtool \
        llvm \
        lsb-release \
        make \
        ninja-build \
        patch \
        pkg-config \
        python3 \
        python3-dev \
        python3-pip \
        tar \
        unzip \
        zip \
        wget \
        zlib1g-dev \
        apt-utils \
        ca-certificates \
        apt-transport-https

# Install all the direct (and indirect) dependencies for google-cloud-cpp.
# Use a different directory for each build, and remove the downloaded
# files and any temporary artifacts after a successful build to keep the
# image smaller (and with fewer layers)

WORKDIR /var/tmp/build/abseil-cpp
RUN curl -sSL https://github.com/abseil/abseil-cpp/archive/20210324.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/googletest
RUN curl -sSL https://github.com/google/googletest/archive/release-1.11.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out/googletest && \
    cmake --build cmake-out/googletest --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/benchmark
RUN curl -sSL https://github.com/google/benchmark/archive/v1.5.6.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE="Release" \
        -DBUILD_SHARED_LIBS=yes \
        -DBENCHMARK_ENABLE_TESTING=OFF \
        -H. -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/crc32c
RUN curl -sSL https://github.com/google/crc32c/archive/1.1.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/nlohmann-json
RUN curl -sSL https://github.com/nlohmann/json/archive/v3.9.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -H. -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/protobuf
RUN curl -sSL https://github.com/protocolbuffers/protobuf/archive/v3.17.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Hcmake -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/c-ares
RUN curl -sSL https://github.com/c-ares/c-ares/archive/refs/tags/cares-1_17_1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -H. -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/re2
RUN curl -sSL https://github.com/google/re2/archive/2020-11-01.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DRE2_BUILD_TESTING=OFF \
        -H. -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/grpc
RUN curl -sSL https://github.com/grpc/grpc/archive/v1.39.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build
