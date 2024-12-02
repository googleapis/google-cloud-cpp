# Copyright 2024 Google LLC
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

FROM opensuse/leap:15
ARG NCPU=4

RUN zypper refresh && \
    zypper install --allow-downgrade -y automake cmake curl gcc gcc-c++ \
        git gzip libtool make ninja patch tar wget


RUN zypper refresh && \
    zypper install --allow-downgrade -y abseil-cpp-devel c-ares-devel \
        libcurl-devel libopenssl-devel libcrc32c-devel nlohmann_json-devel \
        re2-devel

RUN (echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
    tee /etc/ld.so.conf.d/usrlocal.conf
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
ENV PATH=/usr/local/bin:${PATH}

# Install googletest, remove the downloaded files and the temporary artifacts
# after a successful build to keep the image smaller (and with fewer layers)
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/googletest/archive/v1.15.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && cd /var/tmp && rm -fr build

# Download and compile Google microbenchmark support library:
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/benchmark/archive/v1.9.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE="Release" \
        -DBUILD_SHARED_LIBS=yes \
        -DBENCHMARK_ENABLE_TESTING=OFF \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v29.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && cd /var/tmp && rm -fr build

# We need to patch opentelemetry-cpp to work around a compiler bug in (at least)
# GCC 7.x. See https://github.com/open-telemetry/opentelemetry-cpp/issues/1014
# for more details.
WORKDIR /var/tmp/build/opentelemetry-cpp
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.18.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/Stack &GetStack()/Stack \&GetStack() __attribute__((noinline, noclone))/' "api/include/opentelemetry/context/runtime_context.h" && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_ABSEIL=ON \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/grpc
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.67.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.8.2/sccache-v0.8.2-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Update the ld.conf cache in case any libraries were installed in /usr/local/lib*
RUN ldconfig /usr/local/lib*
