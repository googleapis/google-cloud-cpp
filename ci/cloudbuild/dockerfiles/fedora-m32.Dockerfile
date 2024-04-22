# Copyright 2023 Google LLC
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

FROM fedora:39
ARG NCPU=4

# Install the minimal development tools:
RUN dnf makecache && \
    dnf install -y cmake curl diffutils findutils gcc-c++ git make \
        ninja-build patch tar unzip wget which zip

# Fedora 39 includes packages, with recent enough versions, for most of the
# direct dependencies of `google-cloud-cpp`. We will install those.

# First install the "host" (64-bit) version of the protobuf compiler and gRPC
# code generator.
RUN dnf makecache && \
    dnf install -y protobuf-compiler grpc-cpp

# Then install the "target" (32-bit aka i686) versions of the development
# packages for our dependencies.
RUN dnf makecache && \
    dnf install -y \
        c-ares-devel.i686 \
        glibc-devel.i686 \
        gmock-devel.i686 \
        google-benchmark-devel.i686 \
        google-crc32c-devel.i686 \
        grpc-devel.i686 \
        gtest-devel.i686 \
        libcurl-devel.i686 \
        libstdc++-devel.i686 \
        openssl-devel.i686 \
        protobuf-devel.i686 \
        re2-devel.i686 \
        zlib-devel.i686

# Install the Python modules needed to run the storage emulator
RUN dnf makecache && dnf install -y python3-devel
RUN pip3 install --upgrade pip
RUN pip3 install setuptools wheel

# The Cloud Pub/Sub emulator needs Java, and so does `bazel coverage` :shrug:
# Bazel needs the '-devel' version with javac.
RUN dnf makecache && dnf install -y java-latest-openjdk-devel

# Sets root's password to the empty string to enable users to get a root shell
# inside the container with `su -` and no password. Sudo would not work because
# we run these containers as the invoking user's uid, which does not exist in
# the container's /etc/passwd file.
RUN echo 'root:' | chpasswd

# Fedora's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
# when handling `.pc` files with lots of `Requires:` deps, which happens with
# Abseil. If you plan to use `pkg-config` with any of the installed artifacts,
# you may want to use a recent version of the standard `pkg-config` binary. If
# not, `dnf install pkgconfig` should work.
WORKDIR /var/tmp/build/pkgconf
RUN curl -fsSL https://distfiles.ariadne.space/pkgconf/pkgconf-2.2.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr --with-system-libdir=/lib64:/usr/lib64 --with-system-includedir=/usr/include && \
    make -j ${NCPU:-4} && \
    make install && \
    ldconfig && cd /var/tmp && rm -fr build

# The following steps will install libraries and tools in `/usr/local`. By
# default, pkg-config does not search in these directories. Note how this build
# uses /lib/ in contrast to most Fedora-based build that use /lib64/
ENV PKG_CONFIG_PATH=/usr/local/share/pkgconfig:/usr/lib/pkgconfig

# The project depends on the nlohmann_json library. We use CMake to
# install it as this installs the necessary CMake configuration files.
# Note that this is a header-only library, and often installed manually.
# This leaves your environment without support for CMake pkg-config.
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

# There is no Fedora package for opentelemetry-cpp, so we must build it
# from source. We use C++17 to match the language standard that Abseil is
# compiled with. And we use the compiler flags from ci/etc/m32-toolchain.cmake
# to force a 32-bit install.
WORKDIR /var/tmp/build/opentelemetry
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.14.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_CXX_FLAGS=-m32 \
        -DCMAKE_FIND_ROOT_PATH=/usr/ \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_EXAMPLES=OFF \
        -DWITH_ABSEIL=ON \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig

WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.8.0/sccache-v0.8.0-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Install the Cloud SDK and some of the emulators. We use the emulators to run
# integration tests for the client libraries.
COPY . /var/tmp/ci
WORKDIR /var/tmp/downloads
# The Google Cloud CLI requires Python <= 3.10, Fedora defaults to 3.12.
RUN dnf makecache && dnf install -y python3.10
ENV CLOUDSDK_PYTHON=python3.10
RUN /var/tmp/ci/install-cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}

# Update the ld.conf cache in case any libraries were installed in /usr/local/lib*
RUN ldconfig /usr/local/lib*
