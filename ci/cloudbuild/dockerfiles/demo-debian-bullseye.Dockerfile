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

FROM debian:bullseye
ARG NCPU=4

## [BEGIN packaging.md]

# Install the minimal development tools, libcurl, and OpenSSL:

# ```bash
RUN apt-get update && \
    apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ca-certificates ccache cmake curl git \
        gcc g++ libc-ares-dev libc-ares2 libcurl4-openssl-dev libre2-dev \
        libssl-dev m4 make ninja-build pkg-config tar wget zlib1g-dev
# ```

# #### Abseil

# Debian 11 ships with Abseil==20200923.3.  Unfortunately, the current gRPC
# version needs Abseil >= 20210324.

# :warning: By default, Abseil's ABI changes depending on whether it is used
# with C++ >= 17 enabled or not. Installing Abseil with the default
# configuration is error-prone, unless you can guarantee that all the code using
# Abseil (gRPC, google-cloud-cpp, your own code, etc.) is compiled with the same
# C++ version. We recommend that you switch the default configuration to pin
# Abseil's ABI to the version used at compile time. In this case, the compiler
# defaults to C++14. Therefore, we change `absl/base/options.h` to **always**
# use `absl::any`, `absl::string_view`, and `absl::variant`. See
# [abseil/abseil-cpp#696] for more information.

# ```bash
WORKDIR /var/tmp/build/abseil-cpp
RUN curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
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

# Debian 11 also ships with nlohmann-json==3.9.1, which is recent enough for our needs:

# ```bash
RUN apt-get update && \
    apt-get --no-install-recommends install -y nlohmann-json3-dev
# ```

# #### Protobuf

# Unless you are only using the Google Cloud Storage library the project
# needs Protobuf and gRPC. Unfortunately the version of Protobuf that ships
# with Debian 11 is not recent enough to support the protos published by
# Google Cloud. We need to build from source:

# ```bash
WORKDIR /var/tmp/build/protobuf
RUN curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -S . -B cmake-out && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### gRPC

# Finally, we build gRPC from source:

# ```bash
WORKDIR /var/tmp/build/grpc
RUN curl -sSL https://github.com/grpc/grpc/archive/v1.47.0.tar.gz | \
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
        -S . -B cmake-out && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

## [DONE packaging.md]

# Some of the above libraries may have installed in /usr/local, so make sure
# those library directories will be found.
RUN ldconfig /usr/local/lib*
