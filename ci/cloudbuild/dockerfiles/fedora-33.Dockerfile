# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

FROM fedora:33
ARG NCPU=4

# Fedora includes packages for gRPC, libcurl, and OpenSSL that are recent enough
# for `google-cloud-cpp`. Install these packages and additional development
# tools to compile the dependencies:
RUN dnf makecache && \
    dnf install -y abi-compliance-checker abi-dumper autoconf automake ccache \
        clang clang-analyzer clang-tools-extra \
        cmake diffutils doxygen findutils gcc-c++ git \
        grpc-devel grpc-plugins lcov libcxx-devel libcxxabi-devel \
        libasan libubsan libtsan libcurl-devel make ninja-build npm \
        openssl-devel patch pkgconfig protobuf-compiler python python3.8 \
        python-pip ShellCheck tar unzip w3m wget which zip zlib-devel

# Sets root's password to the empty string to enable users to get a root shell
# inside the container with `su -` and no password. Sudo would not work because
# we run these containers as the invoking user's uid, which does not exist in
# the container's /etc/passwd file.
RUN echo 'root:' | chpasswd

# Install the buildifier tool to detect formatting errors in BUILD files.
RUN wget -q -O /usr/bin/buildifier https://github.com/bazelbuild/buildtools/releases/download/0.29.0/buildifier
RUN chmod 755 /usr/bin/buildifier

# Install shfmt to automatically format the Shell scripts.
RUN curl -L -o /usr/local/bin/shfmt \
    "https://github.com/mvdan/sh/releases/download/v3.1.0/shfmt_v3.1.0_linux_amd64" && \
    chmod 755 /usr/local/bin/shfmt

# Install cmake_format to automatically format the CMake list files.
#     https://github.com/cheshirekow/cmake_format
# Pin this to an specific version because the formatting changes when the
# "latest" version is updated, and we do not want the builds to break just
# because some third party changed something.
RUN pip3 install --upgrade pip
RUN pip3 install cmake_format==0.6.8

# Install black to automatically format the Python files.
RUN pip3 install black==19.3b0

# Install the Python modules needed to run the storage emulator
RUN dnf makecache && dnf install -y python3-devel
RUN pip3 install setuptools wheel

# Install cspell for spell checking.
RUN npm install -g cspell@5.2.4

# Install Abseil, remove the downloaded files and the temporary artifacts
# after a successful build to keep the image smaller (and with fewer layers)
WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/abseil/abseil-cpp/archive/20210324.2.tar.gz | \
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
RUN curl -sSL https://github.com/google/googletest/archive/release-1.10.0.tar.gz | \
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
RUN curl -sSL https://github.com/google/benchmark/archive/v1.5.4.tar.gz | \
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
RUN curl -sSL https://github.com/google/crc32c/archive/1.1.0.tar.gz | \
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
RUN curl -sSL https://github.com/nlohmann/json/archive/v3.9.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -H. -Bcmake-out/nlohmann/json && \
    cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU} && \
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

# Install Bazel because some of the builds need it.
RUN /var/tmp/ci/install-bazel.sh

# Some of the above libraries may have installed in /usr/local, so make sure
# those library directories will be found.
RUN ldconfig /usr/local/lib*
