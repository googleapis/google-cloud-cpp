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

FROM centos:7

# First install the development tools and OpenSSL. The development tools
# distributed with CentOS 7 are too old to build the project. In these
# instructions, we use `cmake3` and `gcc-7` obtained from
# [Software Collections](https://www.softwarecollections.org/).

RUN rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
RUN yum install -y centos-release-scl yum-utils
RUN yum-config-manager --enable rhel-server-rhscl-7-rpms
RUN yum makecache && \
    yum install -y automake ccache cmake3 curl-devel devtoolset-7 gcc gcc-c++ \
        git libtool make ninja-build openssl-devel patch re2-devel tar wget \
        which zlib-devel
RUN ln -sf /usr/bin/cmake3 /usr/bin/cmake && ln -sf /usr/bin/ctest3 /usr/bin/ctest

# In order to use the `devtoolset-7` Software Collection we're supposed to run
# `scl enable devtoolset-7 bash`, which starts a new shell with the environment
# configured correctly. However, we can't do that in this Dockerfile, AND we
# want the instructions that we generate for the user to say the right thing.
# So this block is ignored, and we manually set some environment variables to
# make the devtoolset-7 available. After this ignored block, we'll include the
# correct instructions for the user. NOTE: These env values were obtained by
# manually running the `scl ...` command (above) then copying the values set in
# its environment.
ENV PATH /opt/rh/devtoolset-7/root/usr/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ENV LD_LIBRARY_PATH /opt/rh/devtoolset-7/root/usr/lib64:/opt/rh/devtoolset-7/root/usr/lib:/opt/rh/devtoolset-7/root/usr/lib64/dyninst:/opt/rh/devtoolset-7/root/usr/lib/dyninst:/opt/rh/devtoolset-7/root/usr/lib64:/opt/rh/devtoolset-7/root/usr/lib

# CentOS-7 ships with `pkg-config` 0.27.1, which has a
# [bug](https://bugs.freedesktop.org/show_bug.cgi?id=54716) that can make
# invocations take extremely long to complete.

WORKDIR /var/tmp/build/pkg-config-cpp
RUN curl -sSL https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --with-internal-glib && \
    make -j "$(nproc)" && \
    make install && \
    ldconfig && cd /var/tmp && rm -fr build

# The following steps will install libraries and tools in `/usr/local`. By
# default, CentOS-7 does not search for shared libraries in these directories,
# there are multiple ways to solve this problem, the following steps are one
# solution:

RUN (echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
    tee /etc/ld.so.conf.d/usrlocal.conf
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib64/pkgconfig
ENV PATH=/usr/local/bin:${PATH}

WORKDIR /var/tmp/build/abseil-cpp
RUN curl -sSL https://github.com/abseil/abseil-cpp/archive/20220623.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/protobuf
RUN curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.9.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/c-ares
RUN curl -sSL https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./buildconf && ./configure && make -j "$(nproc)" && \
    make install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/grpc
RUN curl -sSL https://github.com/grpc/grpc/archive/v1.49.1.tar.gz | \
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
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/crc32c
RUN curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out && \
    cmake --build cmake-out --target install  && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/json
RUN curl -sSL https://github.com/nlohmann/json/archive/v3.11.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

# Install googletest, remove the downloaded files and the temporary artifacts
# after a successful build to keep the image smaller (and with fewer layers)
WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/google/googletest/archive/release-1.12.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

# Download and compile Google microbenchmark support library:
WORKDIR /var/tmp/build
RUN curl -sSL https://github.com/google/benchmark/archive/v1.7.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE="Release" \
        -DBUILD_SHARED_LIBS=yes \
        -DBENCHMARK_ENABLE_TESTING=OFF \
        -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

RUN ldconfig /usr/local/lib*
