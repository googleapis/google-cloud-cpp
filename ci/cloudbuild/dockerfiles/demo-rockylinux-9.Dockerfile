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
    dnf install -y ccache cmake curl findutils gcc-c++ git make openssl-devel \
        patch re2-devel zlib-devel libcurl-devel c-ares-devel tar wget which
# ```

# Rocky Linux's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is
# slow when handling `.pc` files with lots of `Requires:` deps, which happens
# with Abseil. If you plan to use `pkg-config` with any of the installed
# artifacts, you may want to use a recent version of the standard `pkg-config`
# binary. If not, `dnf install pkgconfig` should work.

# ```bash
WORKDIR /var/tmp/build/pkg-config-cpp
RUN curl -sSL https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --with-internal-glib && \
    make -j ${NCPU:-4} && \
    make install && \
    ldconfig
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
# Abseiil from source.

# :warning: By default, Abseil's ABI changes depending on whether it is used
# with C++ >= 17 enabled or not. Installing Abseil with the default
# configuration is error-prone, unless you can guarantee that all the code using
# Abseil (gRPC, google-cloud-cpp, your own code, etc.) is compiled with the same
# C++ version. We recommend that you switch the default configuration to pin
# Abseil's ABI to the version used at compile time. In Rocky Linux 9 the
# compiler defaults to C++17. Therefore, we change `absl/base/options.h` to
# **always** use `std::any`, `std::string_view`, and `std::variant`. See
# [abseil/abseil-cpp#696] for more information.

# ```bash
WORKDIR /var/tmp/build/abseil-cpp
RUN curl -sSL https://github.com/abseil/abseil-cpp/archive/20220623.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 1/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DABSL_BUILD_TESTING=OFF \
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
RUN curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.10.tar.gz | \
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

# #### gRPC

# We also need a version of gRPC that is recent enough to support the Google
# Cloud Platform proto files. Note that gRPC overrides the default C++ standard
# version to C++14, we need to configure it to use the platform's default. We
# manually install it using:

# ```bash
WORKDIR /var/tmp/build/grpc
RUN curl -sSL https://github.com/grpc/grpc/archive/v1.51.1.tar.gz | \
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
RUN curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
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
RUN curl -sSL https://github.com/nlohmann/json/archive/v3.11.2.tar.gz | \
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

## [DONE packaging.md]

# Some of the above libraries may have installed in /usr/local, so make sure
# those library directories will be found.
RUN ldconfig /usr/local/lib*
