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

FROM fedora:37
ARG NCPU=4

# Install the minimal development tools:
RUN dnf makecache && \
    dnf install -y ccache cmake curl findutils gcc-c++ git make ninja-build \
        patch unzip tar wget zip

# Fedora 37 includes packages, with recent enough versions, for most of the
# direct dependencies of `google-cloud-cpp`. We will install those directly.

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
WORKDIR /var/tmp/build/pkg-config-cpp
RUN curl -sSL https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --with-internal-glib && \
    make -j ${NCPU:-4} && \
    make install && \
    ldconfig

# The version of RE2 included with this distro hard-codes C++11 in its
# pkg-config file. This is fixed in later versions, and it is unnecessary as
# Fedora's compiler defaults to C++17.
RUN sed -i 's/-std=c\+\+11 //' /usr/lib/pkgconfig/re2.pc

# The following steps will install libraries and tools in `/usr/local`. By
# default, pkg-config does not search in these directories. Note how this build
# uses /lib/ in contrast to most Fedora-based build that use /lib64/
ENV PKG_CONFIG_PATH=/usr/local/share/pkgconfig:/usr/lib/pkgconfig

# The project depends on the nlohmann_json library. We use CMake to
# install it as this installs the necessary CMake configuration files.
# Note that this is a header-only library, and often installed manually.
# This leaves your environment without support for CMake pkg-config.
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

# Install the Cloud SDK and some of the emulators. We use the emulators to run
# integration tests for the client libraries.
COPY . /var/tmp/ci
WORKDIR /var/tmp/downloads
ENV CLOUDSDK_PYTHON=python3
RUN /var/tmp/ci/install-cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}

# Some of the above libraries may have installed in /usr/local, so make sure
# those library directories will be found.
RUN ldconfig /usr/local/lib*
