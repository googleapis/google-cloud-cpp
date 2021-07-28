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

FROM centos:7
ARG NCPU=4

## [BEGIN packaging.md]

# First install the development tools and OpenSSL. The development tools
# distributed with CentOS 7 are too old to build the project. In these
# instructions, we use `cmake3` and `gcc-7` obtained from
# [Software Collections](https://www.softwarecollections.org/).

# ```bash
RUN rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
RUN yum install -y centos-release-scl yum-utils
RUN yum-config-manager --enable rhel-server-rhscl-7-rpms
RUN yum makecache && \
    yum install -y automake ccache cmake3 curl-devel devtoolset-7 gcc gcc-c++ \
        git libtool make openssl-devel patch re2-devel tar wget which zlib-devel
RUN ln -sf /usr/bin/cmake3 /usr/bin/cmake && ln -sf /usr/bin/ctest3 /usr/bin/ctest
# ```

## [BEGIN IGNORED]
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
## [DONE IGNORED]
# Start a bash shell with its environment configured to use the tools installed
# by `devtoolset-7`.
# **IMPORTANT**: All the following commands should be run from this new shell.
# ```bash
# scl enable devtoolset-7 bash
# ```

# CentOS-7 ships with `pkg-config` 0.27.1, which has a
# [bug](https://bugs.freedesktop.org/show_bug.cgi?id=54716) that can make
# invocations take extremely long to complete. If you plan to use `pkg-config`
# with any of the installed artifacts, you'll want to upgrade it to something
# newer. If not, `yum install pkgconfig` should work instead.

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
# default CentOS-7 does not search for shared libraries in these directories,
# there are multiple ways to solve this problem, the following steps are one
# solution:

# ```bash
RUN (echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
    tee /etc/ld.so.conf.d/usrlocal.conf
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib64/pkgconfig
ENV PATH=/usr/local/bin:${PATH}
# ```

# #### Abseil

# We need a recent version of Abseil.

# ```bash
WORKDIR /var/tmp/build/abseil-cpp
RUN curl -sSL https://github.com/abseil/abseil-cpp/archive/20210324.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### Protobuf

# We need to install a version of Protobuf that is recent enough to support the
# Google Cloud Platform proto files:

# ```bash
WORKDIR /var/tmp/build/protobuf
RUN curl -sSL https://github.com/protocolbuffers/protobuf/archive/v3.17.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Hcmake -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### c-ares

# Recent versions of gRPC require c-ares >= 1.11, while CentOS-7
# distributes c-ares-1.10. Manually install a newer version:

# ```bash
WORKDIR /var/tmp/build/c-ares
RUN curl -sSL https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
    make install && \
    ldconfig
# ```

# #### gRPC

# We also need a version of gRPC that is recent enough to support the Google
# Cloud Platform proto files. We manually install it using:

# ```bash
WORKDIR /var/tmp/build/grpc
RUN curl -sSL https://github.com/grpc/grpc/archive/v1.39.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### crc32c

# The project depends on the Crc32c library, we need to compile this from
# source:

# ```bash
WORKDIR /var/tmp/build/crc32c
RUN curl -sSL https://github.com/google/crc32c/archive/1.1.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
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
RUN curl -sSL https://github.com/nlohmann/json/archive/v3.9.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -H. -Bcmake-out/nlohmann/json && \
    cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

## [DONE packaging.md]

# Some of the above libraries may have installed in /usr/local, so make sure
# those library directories will be found.
RUN ldconfig /usr/local/lib*
