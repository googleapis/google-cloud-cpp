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

# Fedora includes packages for libcurl and OpenSSL that are recent enough
# for `google-cloud-cpp`. Install these packages and additional development
# tools to compile the dependencies:
RUN dnf makecache && \
    dnf install -y abi-compliance-checker autoconf automake \
        clang clang-analyzer clang-tools-extra \
        cmake diffutils findutils gcc-c++ git \
        libcurl-devel llvm make ninja-build \
        openssl-devel patch python python3 \
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
RUN dnf makecache && dnf install -y pugixml-devel yaml-cpp-devel

# Sets root's password to the empty string to enable users to get a root shell
# inside the container with `su -` and no password. Sudo would not work because
# we run these containers as the invoking user's uid, which does not exist in
# the container's /etc/passwd file.
RUN echo 'root:' | chpasswd

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
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig

# We disable the inline namespace because otherwise Abseil LTS updates break our
# `check-api` build.
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20240116.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    sed -i 's/^#define ABSL_OPTION_USE_INLINE_NAMESPACE 1$/#define ABSL_OPTION_USE_INLINE_NAMESPACE 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DABSL_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/googletest/archive/v1.14.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/benchmark/archive/v1.8.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -DBENCHMARK_ENABLE_TESTING=OFF \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
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
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v26.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/grpc
RUN dnf makecache && dnf install -y c-ares-devel re2-devel
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.62.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
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
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.15.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=14 \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_EXAMPLES=OFF \
        -DWITH_ABSEIL=ON \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
    -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

# Installs Universal Ctags (which is different than the default "Exuberant
# Ctags"), which is needed by the ABI checker. See https://ctags.io/
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/universal-ctags/ctags/archive/refs/tags/p5.9.20210418.0.tar.gz | \
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
RUN curl -fsSL https://github.com/lvc/abi-dumper/archive/16bb467cd7d343dd3a16782b151b56cf15509594.tar.gz | \
    tar -xzf - --strip-components=1 && \
    mv abi-dumper.pl /usr/local/bin/abi-dumper && \
    chmod +x /usr/local/bin/abi-dumper

# Install ctcache (with global caching support) to speed up our clang-tidy build
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/matus-chochlik/ctcache/archive/62631eb1c05688f79f8cd652fe4d726f09bb1eb3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    pip3 install --quiet --disable-pip-version-check google-cloud-storage && \
    pip3 install --quiet --disable-pip-version-check -r requirements.txt && \
    cp clang-tidy /usr/local/bin/clang-tidy-wrapper && \
    cp clang-tidy-cache /usr/local/bin/clang-tidy-cache && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.8.0/sccache-v0.8.0-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Update the ld.conf cache in case any libraries were installed in /usr/local/lib*
RUN (echo /usr/local/lib; echo /usr/local/lib64) | tee /etc/ld.so.conf.d/local.conf
RUN ldconfig /usr/local/lib*

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
