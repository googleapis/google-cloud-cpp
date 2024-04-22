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
ARG ARCH=amd64

# Fedora includes packages for gRPC, libcurl, and OpenSSL that are recent enough
# for `google-cloud-cpp`. Install these packages and additional development
# tools to compile the dependencies:
RUN dnf makecache && \
    dnf install -y autoconf automake \
        clang cmake diffutils findutils gcc-c++ git \
        make ninja-build patch python3 \
        python-pip tar unzip wget which zip zlib-devel

# Install the development packages for libcurl and OpenSSL. Neither are affected
# by the C++ version, so we can use the pre-built binaries.
RUN dnf makecache && \
    dnf install -y libcurl-devel openssl-devel

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
# Abseil, so we use the normal `pkg-config` binary, which seems to not suffer
# from this bottleneck. For more details see
# https://github.com/googleapis/google-cloud-cpp/issues/7052
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
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig

# Download and install direct dependencies of `google-cloud-cpp`. Including
# development dependencies.  In each case, remove the downloaded files and the
# temporary artifacts after a successful build to keep the image smaller (and
# with fewer layers).

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20240116.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_CXX_STANDARD=20 \
      -DCMAKE_BUILD_TYPE=Release \
      -DABSL_BUILD_TESTING=OFF \
      -DABSL_PROPAGATE_CXX_STD=ON \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/googletest/archive/v1.14.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_CXX_STANDARD=20 \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/benchmark/archive/v1.8.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DBENCHMARK_ENABLE_TESTING=OFF \
        -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_CXX_STANDARD=20 \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/nlohmann/json/archive/v3.11.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_CXX_STANDARD=20 \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v26.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/grpc
RUN dnf makecache && dnf install -y c-ares-devel re2-devel
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.62.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=20 \
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
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.15.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_EXAMPLES=OFF \
        -DWITH_ABSEIL=ON \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
    -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && cmake --install cmake-out && \
    ldconfig && cd /var/tmp && rm -fr build

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
