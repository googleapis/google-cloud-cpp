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

FROM fedora:36
ARG NCPU=4

# Fedora includes packages for gRPC, libcurl, and OpenSSL that are recent enough
# for `google-cloud-cpp`. Install these packages and additional development
# tools to compile the dependencies:
RUN dnf makecache && \
    dnf install -y abi-compliance-checker autoconf automake \
        ccache clang clang-analyzer clang-tools-extra \
        cmake diffutils doxygen findutils gcc-c++ git \
        lcov libcxx-devel libcxxabi-devel \
        libasan libubsan libtsan libcurl-devel make ninja-build \
        openssl-devel patch python python3.8 \
        python-pip tar unzip w3m wget which zip zlib-devel

# Sets root's password to the empty string to enable users to get a root shell
# inside the container with `su -` and no password. Sudo would not work because
# we run these containers as the invoking user's uid, which does not exist in
# the container's /etc/passwd file.
RUN echo 'root:' | chpasswd

# Install the Python modules needed to run the storage emulator
RUN dnf makecache && dnf install -y python3-devel
RUN pip3 install --upgrade pip
RUN pip3 install setuptools wheel

# Fedora's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
# when handling `.pc` files with lots of `Requires:` deps, which happens with
# Abseil, so we use the normal `pkg-config` binary, which seems to not suffer
# from this bottleneck. For more details see
# https://github.com/googleapis/google-cloud-cpp/issues/7052
WORKDIR /var/tmp/build/pkg-config-cpp
RUN curl -sSL https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --with-internal-glib && \
    make -j ${NCPU:-4} && \
    make install && \
    ldconfig
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig

# Install Abseil, remove the downloaded files and the temporary artifacts
# after a successful build to keep the image smaller (and with fewer layers)
WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
    cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

# Install googletest, remove the downloaded files and the temporary artifacts
# after a successful build to keep the image smaller (and with fewer layers)
WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/google/googletest/archive/release-1.11.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/googletest && \
    cmake --build cmake-out/googletest --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

# Download and compile Google microbenchmark support library:
WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/google/benchmark/archive/v1.6.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE="Release" \
        -DBUILD_SHARED_LIBS=yes \
        -DBENCHMARK_ENABLE_TESTING=OFF \
        -H. -Bcmake-out/benchmark && \
    cmake --build cmake-out/benchmark --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out/crc32c && \
    cmake --build cmake-out/crc32c --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/nlohmann/json/archive/v3.10.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -H. -Bcmake-out/nlohmann/json && \
    cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/protobuf
RUN curl -sSL https://github.com/protocolbuffers/protobuf/archive/v3.19.4.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Hcmake -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/grpc
RUN dnf makecache && dnf install -y c-ares-devel re2-devel
RUN curl -sSL https://github.com/grpc/grpc/archive/v1.45.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
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
        -H. -Bcmake-out -GNinja && \
    cmake --build cmake-out --target install -- -j ${NCPU} && \
    ldconfig && \
    cd /var/tmp && rm -fr build

# Install ctcache to speed up our clang-tidy build
WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/matus-chochlik/ctcache/archive/0ad2e227e8a981a9c1a6060ee6c8ec144bb976c6.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cp clang-tidy /usr/local/bin/clang-tidy-wrapper && \
    cp clang-tidy-cache /usr/local/bin/clang-tidy-cache && \
    cd /var/tmp && rm -fr build

# Installs Universal Ctags (which is different than the default "Exuberant
# Ctags"), which is needed by the ABI checker. See https://ctags.io/
WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/universal-ctags/ctags/archive/refs/tags/p5.9.20210418.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./autogen.sh && \
    ./configure --prefix=/usr/local && \
    make && \
    make install && \
    cd /var/tmp && rm -fr build

# Installs the abi-dumper with the integer overflow fix from
# https://github.com/lvc/abi-dumper/pull/29. We can switch back to `dnf install
# abi-dumper` once it has the fix.
WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/lvc/abi-dumper/archive/814effec0f20a9613441dfa033aa0a0bc2a96a87.tar.gz | \
    tar -xzf - --strip-components=1 && \
    mv abi-dumper.pl /usr/local/bin/abi-dumper && \
    chmod +x /usr/local/bin/abi-dumper

# Install the Cloud SDK and some of the emulators. We use the emulators to run
# integration tests for the client libraries.
COPY . /var/tmp/ci
WORKDIR /var/tmp/downloads
ENV CLOUDSDK_PYTHON=python3.8
RUN /var/tmp/ci/install-cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}
# The Cloud Pub/Sub emulator needs Java, and so does `bazel coverage` :shrug:
# Bazel needs the '-devel' version with javac.
RUN dnf makecache && dnf install -y java-latest-openjdk-devel

# Some of the above libraries may have installed in /usr/local, so make sure
# those library directories will be found.
RUN ldconfig /usr/local/lib*

RUN curl -o /usr/bin/bazelisk -sSL "https://github.com/bazelbuild/bazelisk/releases/download/v1.11.0/bazelisk-linux-amd64" && \
    chmod +x /usr/bin/bazelisk && \
    ln -s /usr/bin/bazelisk /usr/bin/bazel
