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

FROM ubuntu:jammy
ARG NCPU=4

## [BEGIN packaging.md]

# Install the minimal development tools, OpenSSL and libc-ares:

# ```bash
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential cmake ca-certificates curl git \
        gcc g++ libc-ares-dev libc-ares2 libnghttp2-dev libre2-dev \
        libssl-dev m4 make pkg-config tar wget zlib1g-dev libc6-dbg
# ```

# #### curl
#
# Install curl from source to avoid a libcurl bug present in older versions.
# See https://github.com/googleapis/google-cloud-cpp/issues/16343 for more
# details.
#
# ```bash
WORKDIR /var/tmp/build/curl
RUN curl -fsSL https://github.com/curl/curl/releases/download/curl-8_7_1/curl-8.7.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed 's/@CURL_LT_SHLIB_VERSIONED_FLAVOUR@/OPENSSL_/g' lib/libcurl.vers.in > libcurl.vers && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=ON \
        -DCURL_USE_OPENSSL=ON \
        -DBUILD_CURL_EXE=ON \
        -DBUILD_TESTING=OFF \
        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--version-script=$(pwd)/libcurl.vers" \
        -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build
# ```

# #### Abseil

# ```bash
WORKDIR /var/tmp/build/abseil-cpp
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20250814.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_STANDARD=17 \
      -DABSL_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### Protobuf

# We need to install a version of Protobuf that is recent enough to support the
# Google Cloud Platform proto files:

# ```bash
WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v36.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### gRPC

# We also need a version of gRPC that is recent enough to support the Google
# Cloud Platform proto files. We install it using:

# ```bash
WORKDIR /var/tmp/build/grpc
## [BEGIN IGNORED]
## The [BEGIN IGNORED] / [DONE IGNORED] sections below are omitted when
## generating doc/packaging.md via ci/generate-markdown/generate-packaging.sh.
## They work around an upstream gRPC + OpenSSL issue
## (https://github.com/grpc/grpc/issues/42068) in CI by building gRPC against
## its bundled BoringSSL (-DgRPC_SSL_PROVIDER=module) with prefixed symbols
## (BORINGSSL_PREFIX=bssl_grpc) and renamed shared libraries (bssl_crypto,
## bssl_ssl) so they do not collide with system OpenSSL (libssl.so, libcrypto.so)
## at build or run time.
RUN apt-get update && apt-get --no-install-recommends install -y golang-go
## [DONE IGNORED]
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.84.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    mkdir -p third_party/grpc-proto && \
    curl -fsSL https://github.com/grpc/grpc-proto/archive/ec30f589e2519d595688b9a42f88a91bdd6b733f.tar.gz | \
    tar -xzf - -C third_party/grpc-proto --strip-components=1 && \
## [BEGIN IGNORED]
## Build BoringSSL once to extract its symbol list with Go, disable its CMake
## install rules (to avoid overwriting system OpenSSL headers/pkg-config), and
## rename its shared libraries so they can be installed alongside OpenSSL.
    mkdir -p third_party/boringssl-with-bazel && \
    curl -fsSL https://github.com/google/boringssl/archive/2b44a3701a4788e1ef866ddc7f143060a3d196c9.tar.gz | \
    tar -xzf - -C third_party/boringssl-with-bazel --strip-components=1 && \
    printf 'module boringssl.googlesource.com/boringssl.git\n\ngo 1.18\n' > third_party/boringssl-with-bazel/go.mod && \
    cmake -S third_party/boringssl-with-bazel -B bssl-build && \
    cmake --build bssl-build --target crypto ssl -- -j ${NCPU:-4} && \
    (cd third_party/boringssl-with-bazel && go run util/read_symbols.go -out ../../bssl_symbols.txt ../../bssl-build/libcrypto.a ../../bssl-build/libssl.a) && \
    rm -rf bssl-build && \
    sed -i 's/set(INSTALL_ENABLED 1)/set(INSTALL_ENABLED 0)/' third_party/boringssl-with-bazel/CMakeLists.txt && \
    printf '%s\n' \
        'set_target_properties(crypto PROPERTIES OUTPUT_NAME bssl_crypto)' \
        'set_target_properties(ssl PROPERTIES OUTPUT_NAME bssl_ssl)' \
        'add_dependencies(ssl boringssl_prefix_symbols)' \
        'target_compile_definitions(crypto PUBLIC $<BUILD_INTERFACE:BORINGSSL_PREFIX=${BORINGSSL_PREFIX}>)' \
        'target_include_directories(crypto PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/symbol_prefix_include>)' \
        >> third_party/boringssl-with-bazel/CMakeLists.txt && \
## [DONE IGNORED]
    cmake \
## [BEGIN IGNORED]
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBORINGSSL_PREFIX=bssl_grpc \
        -DBORINGSSL_PREFIX_SYMBOLS="$(pwd)/bssl_symbols.txt" \
## [DONE IGNORED]
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
## [BEGIN IGNORED]
        -DgRPC_SSL_PROVIDER=module \
## [DONE IGNORED]
## Emit -DgRPC_SSL_PROVIDER=package into doc/packaging.md while ignoring it in Docker.
#         -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
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
      -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -S . -B cmake-out && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### opentelemetry-cpp

# ```bash
WORKDIR /var/tmp/build/opentelemetry-cpp
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.24.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_STL=CXX17 \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

## [DONE packaging.md]

RUN apt-get update && \
    apt-get --no-install-recommends install -y gdb

WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.15.0/sccache-v0.15.0-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Update the ld.conf cache in case any libraries were installed in /usr/local/lib*
RUN ldconfig /usr/local/lib*

ENV DEMO_CORD_WORKAROUND=OFF
