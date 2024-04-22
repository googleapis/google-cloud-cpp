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

FROM rockylinux/rockylinux:9
ARG NCPU=4

## [BEGIN packaging.md]

# Install the minimal development tools, libcurl, OpenSSL, and the c-ares
# library (required by gRPC):

# ```bash
RUN dnf makecache && \
    dnf update -y && \
    dnf install -y epel-release && \
    dnf makecache && \
    dnf install -y cmake findutils gcc-c++ git make openssl-devel \
        patch zlib-devel libcurl-devel c-ares-devel tar wget which
# ```

# Rocky Linux's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is
# slow when handling `.pc` files with lots of `Requires:` deps, which happens
# with Abseil. If you plan to use `pkg-config` with any of the installed
# artifacts, you may want to use a recent version of the standard `pkg-config`
# binary. If not, `dnf install pkgconfig` should work.

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
# default, Rocky Linux 9 does not search for shared libraries in these
# directories, there are multiple ways to solve this problem, the following
# steps are one solution:

# ```bash
RUN (echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
    tee /etc/ld.so.conf.d/usrlocal.conf
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib64/pkgconfig
ENV PATH=/usr/local/bin:${PATH}
# ```

# #### Abseil

# Rocky Linux 9 includes a package for Abseil, unfortunately, this package is
# incomplete, as it lacks the CMake support files for it. We need to compile
# Abseiil from source. Enabling `ABSL_PROPAGATE_CXX_STD` propagates the version
# of C++ used to compile Abseil to anything that depends on Abseil.

# ```bash
WORKDIR /var/tmp/build/abseil-cpp
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20240116.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DABSL_BUILD_TESTING=OFF \
      -DABSL_PROPAGATE_CXX_STD=ON \
      -DBUILD_SHARED_LIBS=yes \
      -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### Protobuf

# Rocky Linux ships with Protobuf 3.14.x.  Some of the libraries in
# `google-cloud-cpp` require Protobuf >= 3.15.8.  For simplicity, we will just
# install Protobuf (and any downstream packages) from source.

# ```bash
WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v26.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### RE2

# The version of RE2 included with this distro hard-codes C++11 in its
# pkg-config file. You can skip this build and use the system's package if
# you are not planning to use pkg-config.

# ```bash
WORKDIR /var/tmp/build/re2
RUN curl -fsSL https://github.com/google/re2/archive/2024-04-01.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DRE2_BUILD_TESTING=OFF \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### gRPC

# We also need a version of gRPC that is recent enough to support the Google
# Cloud Platform proto files. Note that gRPC overrides the default C++ standard
# version to C++14, we need to configure it to use the platform's default. We
# manually install it using:

# ```bash
WORKDIR /var/tmp/build/grpc
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.62.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=17 \
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
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
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
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -S . -B cmake-out && \
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
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.15.0.tar.gz | \
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
