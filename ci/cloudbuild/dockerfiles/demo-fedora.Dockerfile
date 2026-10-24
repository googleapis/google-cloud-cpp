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

FROM fedora:40
ARG NCPU=4

## [BEGIN packaging.md]

# Install the minimal development tools:

# ```bash
RUN dnf makecache && \
    dnf install -y cmake curl findutils gcc-c++ git make ninja-build \
        patch unzip tar wget zip dnf-utils zlib-devel
RUN dnf makecache && dnf debuginfo-install -y glibc
# ```

# Fedora:40 includes packages, with recent enough versions, for most of the
# direct dependencies of `google-cloud-cpp`.

# ```bash
RUN dnf makecache && \
    dnf install -y json-devel libnghttp2-devel openssl-devel
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

# The following steps will install libraries and tools in `/usr/local`. By
# default, pkgconf does not search in these directories. We need to explicitly
# set the search path.

# ```bash
ENV PKG_CONFIG_PATH=/usr/local/share/pkgconfig:/usr/lib64/pkgconfig:/usr/local/lib64/pkgconfig
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
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=ON \
        -DCURL_USE_OPENSSL=ON \
        -DBUILD_CURL_EXE=ON \
        -DBUILD_TESTING=OFF \
        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--version-script=$(pwd)/libcurl.vers" \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build
# ```

# #### Protobuf

# ```bash
WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v36.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build
# ```

# #### opentelemetry-cpp

# ```bash
WORKDIR /var/tmp/build/opentelemetry-cpp
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.24.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
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

# #### gRPC

# ```bash
WORKDIR /var/tmp/build/grpc
RUN dnf makecache && dnf install -y c-ares-devel re2-devel
## [BEGIN IGNORED]
## The [BEGIN IGNORED] / [DONE IGNORED] sections below are omitted when
## generating doc/packaging.md via ci/generate-markdown/generate-packaging.sh.
## They work around an upstream gRPC + OpenSSL issue
## (https://github.com/grpc/grpc/issues/42068) in CI by building gRPC against
## its bundled BoringSSL (-DgRPC_SSL_PROVIDER=module) with prefixed symbols
## (BORINGSSL_PREFIX=bssl_grpc) and renamed shared libraries (bssl_crypto,
## bssl_ssl) so they do not collide with system OpenSSL (libssl.so, libcrypto.so)
## at build or run time.
RUN dnf makecache && dnf install -y golang
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
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_STANDARD=17 \
      -DBUILD_SHARED_LIBS=ON \
      -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_ABSL_PROVIDER=package \
      -DgRPC_CARES_PROVIDER=package \
      -DgRPC_PROTOBUF_PROVIDER=package \
      -DgRPC_PROTOBUF_PACKAGE_TYPE=CONFIG \
      -DgRPC_RE2_PROVIDER=package \
## [BEGIN IGNORED]
      -DgRPC_SSL_PROVIDER=module \
## [DONE IGNORED]
## Emit -DgRPC_SSL_PROVIDER=package into doc/packaging.md while ignoring it in Docker.
#       -DgRPC_SSL_PROVIDER=package \
      -DgRPC_ZLIB_PROVIDER=package \
      -DgRPC_OPENTELEMETRY_PROVIDER=package \
      -DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=ON \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build
# ```

## [DONE packaging.md]

RUN dnf makecache && dnf install -y gdb

WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.15.0/sccache-v0.15.0-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Update the ld.conf cache in case any libraries were installed in /usr/local/lib*
RUN (echo /usr/local/lib; echo /usr/local/lib64) | tee /etc/ld.so.conf.d/local.conf
RUN ldconfig /usr/local/lib*
