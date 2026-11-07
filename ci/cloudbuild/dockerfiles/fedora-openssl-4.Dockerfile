# Copyright 2026 Google LLC
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
ARG ARCH=amd64

# Fedora includes packages for OpenSSL that are recent enough
# for `google-cloud-cpp`. Install these packages and additional development
# tools to compile the dependencies:
RUN dnf makecache && \
    dnf install -y abi-compliance-checker autoconf automake \
        clang clang-analyzer clang-tools-extra \
        cmake diffutils findutils gcc-c++ git \
        libnghttp2-devel llvm make ninja-build \
        openssl-devel patch perl-core python python3 \
        python-pip tar unzip w3m wget which zip zlib-devel

# Install the Python modules needed to run the storage emulator
RUN dnf makecache && dnf install -y python3-devel
RUN pip3 install --upgrade pip
RUN pip3 install setuptools wheel

# The Cloud Pub/Sub emulator needs Java :shrug:
RUN dnf makecache && dnf install -y java-latest-openjdk

# This is used to improve the output in check-api builds
RUN dnf makecache && dnf install -y "dnf-command(debuginfo-install)"
RUN dnf makecache && dnf debuginfo-install -y libstdc++

# These are used by the docfx tool.
RUN dnf makecache && dnf install -y pugixml-devel

RUN dnf install -y valgrind

# Sets root's password to the empty string to enable users to get a root shell
# inside the container with `su -` and no password. Sudo would not work because
# we run these containers as the invoking user's uid, which does not exist in
# the container's /etc/passwd file.
RUN echo "root:cloudcxx" | chpasswd

# Build and install OpenSSL 4.0.2 in /usr/local/openssl-4
WORKDIR /var/tmp/build/openssl
RUN curl -fsSL https://github.com/openssl/openssl/releases/download/openssl-4.0.2/openssl-4.0.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./Configure --prefix=/usr/local/openssl-4 --openssldir=/usr/local/openssl-4 && \
    make -j ${NCPU:-4} && \
    make install_sw && \
    cd /var/tmp && rm -fr build
RUN (echo /usr/local/openssl-4/lib; echo /usr/local/openssl-4/lib64; echo /usr/local/lib; echo /usr/local/lib64) | tee /etc/ld.so.conf.d/local.conf && ldconfig

WORKDIR /var/tmp/build/
RUN curl -fsSL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.9.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=ON \
        -DBUILD_TESTING=OFF \
        -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

# Fedora's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
# when handling `.pc` files with lots of `Requires:` deps.  This problem is
# triggered by the Abseil `.pc` files, which we use (indirectly) when testing
# our own `.pc` files.  We install the more traditional `pkg-config` binary.
# For more details see
#     https://github.com/googleapis/google-cloud-cpp/issues/7052
WORKDIR /var/tmp/build/pkgconf
RUN curl -fsSL https://distfiles.ariadne.space/pkgconf/pkgconf-2.2.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr --with-system-libdir=/lib64:/usr/lib64 --with-system-includedir=/usr/include && \
    make -j ${NCPU:-4} && \
    make install && \
    ldconfig && cd /var/tmp && rm -fr build

# The following steps will install libraries and tools in `/usr/local`. By
# default, pkgconf does not search in these directories. We need to explicitly
# set the search path.
ENV PKG_CONFIG_PATH=/usr/local/openssl-4/lib64/pkgconfig:/usr/local/openssl-4/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig

# Install curl from source to avoid a libcurl bug present in older versions.
# See https://github.com/googleapis/google-cloud-cpp/issues/16343 for more
# details.
WORKDIR /var/tmp/build/curl
RUN curl -fsSL https://github.com/curl/curl/releases/download/curl-8_22_0/curl-8.22.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    printf "CURL_OPENSSL_4\n{\n  global: curl_*;\n  local: *;\n};\n" > libcurl.vers && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=ON \
        -DOPENSSL_ROOT_DIR=/usr/local/openssl-4 \
        -DCURL_USE_OPENSSL=ON \
        -DCURL_USE_LIBPSL=OFF \
        -DBUILD_CURL_EXE=ON \
        -DBUILD_TESTING=OFF \
        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--version-script=$(pwd)/libcurl.vers" \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

# We disable the inline namespace because otherwise Abseil LTS updates break our
# `check-api` build.
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20250814.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DCMAKE_CXX_STANDARD=17 \
      -DABSL_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/googletest/archive/v1.18.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/benchmark/archive/v1.9.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -DBENCHMARK_ENABLE_TESTING=OFF \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/nlohmann/json/archive/v3.11.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v36.0.tar.gz | \
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

WORKDIR /var/tmp/build/
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.28.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_EXAMPLES=OFF \
        -DWITH_STL=CXX17 \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
    -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/grpc
RUN dnf makecache && dnf install -y c-ares-devel re2-devel
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.84.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    mkdir -p third_party/grpc-proto && \
    curl -fsSL https://github.com/grpc/grpc-proto/archive/ec30f589e2519d595688b9a42f88a91bdd6b733f.tar.gz | \
    tar -xzf - -C third_party/grpc-proto --strip-components=1 && \
    PKG_CONFIG_PATH=/usr/lib64/pkgconfig:/usr/share/pkgconfig cmake \
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
      -DgRPC_SSL_PROVIDER=package \
      -DgRPC_ZLIB_PROVIDER=package \
      -DgRPC_OPENTELEMETRY_PROVIDER=package \
      -DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=ON \
      -DOPENSSL_ROOT_DIR=/usr \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.15.0/sccache-v0.15.0-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Update the dynamic linker cache for all installed libraries
RUN ldconfig
