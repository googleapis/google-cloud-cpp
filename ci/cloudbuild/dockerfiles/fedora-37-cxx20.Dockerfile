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
ARG ARCH=amd64

# Fedora includes packages for gRPC, libcurl, and OpenSSL that are recent enough
# for `google-cloud-cpp`. Install these packages and additional development
# tools to compile the dependencies:
RUN dnf makecache && \
    dnf install -y autoconf automake \
        ccache clang clang-analyzer clang-tools-extra \
        cmake diffutils findutils gcc-c++ git \
        libcurl-devel llvm make ninja-build \
        openssl-devel patch python python3 \
        python-pip tar unzip wget which zip zlib-devel

# Install the Python modules needed to run the storage emulator
RUN dnf makecache && dnf install -y python3-devel
RUN pip3 install --upgrade pip
RUN pip3 install setuptools wheel

# This is needed to install gRPC
RUN dnf makecache && dnf install -y c-ares-devel

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
WORKDIR /var/tmp/build/pkg-config-cpp
RUN curl -fsSL https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --with-internal-glib && \
    make -j ${NCPU:-4} && \
    make install && \
    ldconfig
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig

# Install Abseil, remove the downloaded files and the temporary artifacts
# after a successful build to keep the image smaller (and with fewer layers)
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20230125.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_CXX_STANDARD=20 \
      -DCMAKE_BUILD_TYPE="Release" \
      -DABSL_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

# Install googletest, remove the downloaded files and the temporary artifacts
# after a successful build to keep the image smaller (and with fewer layers)
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/googletest/archive/v1.13.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_CXX_STANDARD=20 \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

# Download and compile Google microbenchmark support library:
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/benchmark/archive/v1.8.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DBENCHMARK_ENABLE_TESTING=OFF \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
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
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/nlohmann/json/archive/v3.11.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_CXX_STANDARD=20 \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v23.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

# The version of RE2 installed with Fedora:37 forces C++11 in its pkg-config
# files. This may be fixed in Fedora:38, but until then it is easier to just
# install the source code.
WORKDIR /var/tmp/build/re2
RUN curl -fsSL https://github.com/google/re2/archive/2023-06-02.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DRE2_BUILD_TESTING=OFF \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig

WORKDIR /var/tmp/build/grpc
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.55.0.tar.gz | \
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
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.9.1.tar.gz | \
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
        -H. -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

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

RUN curl -o /usr/bin/bazelisk -sSL "https://github.com/bazelbuild/bazelisk/releases/download/v1.17.0/bazelisk-linux-${ARCH}" && \
    chmod +x /usr/bin/bazelisk && \
    ln -s /usr/bin/bazelisk /usr/bin/bazel
