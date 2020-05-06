#!/usr/bin/env bash
# Copyright 2019 Google LLC
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

read_into_variable WARNING_GENERATED_FILE_FRAGMENT <<'_EOF_'
#
# WARNING: This is an automatically generated file. Consider changing the
#     sources instead. You can find the source templates and scripts at:
#     https://github.com/googleapis/google-cloud-cpp/tree/master/ci/templates
#
_EOF_

read_into_variable INSTALL_CPP_CMAKEFILES_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/googleapis/cpp-cmakefiles/archive/v0.9.0.tar.gz && \
    tar -xf v0.9.0.tar.gz && \
    cd cpp-cmakefiles-0.9.0 && \
    cmake -DBUILD_SHARED_LIBS=YES -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
_EOF_

read_into_variable INSTALL_GOOGLETEST_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/google/googletest/archive/release-1.10.0.tar.gz && \
    tar -xf release-1.10.0.tar.gz && \
    cd googletest-release-1.10.0 && \
    cmake -DCMAKE_BUILD_TYPE="Release" -DBUILD_SHARED_LIBS=yes -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
_EOF_

read_into_variable INSTALL_GOOGLE_BENCHMARK_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/google/benchmark/archive/v1.5.0.tar.gz && \
    tar -xf v1.5.0.tar.gz && \
    cd benchmark-1.5.0 && \
    cmake \
        -DCMAKE_BUILD_TYPE="Release" \
        -DBUILD_SHARED_LIBS=yes \
        -DBENCHMARK_ENABLE_TESTING=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
_EOF_

read_into_variable INSTALL_CRC32C_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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
_EOF_

read_into_variable INSTALL_PROTOBUF_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/google/protobuf/archive/v3.11.3.tar.gz && \
    tar -xf v3.11.3.tar.gz && \
    cd protobuf-3.11.3/cmake && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
_EOF_

read_into_variable INSTALL_C_ARES_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz && \
    tar -xf cares-1_14_0.tar.gz && \
    cd c-ares-cares-1_14_0 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
    make install && \
    ldconfig
_EOF_

read_into_variable INSTALL_GRPC_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/grpc/grpc/archive/78ace4cd5dfcc1f2eced44d22d752f103f377e7b.tar.gz && \
    tar -xf 78ace4cd5dfcc1f2eced44d22d752f103f377e7b.tar.gz && \
    cd grpc-78ace4cd5dfcc1f2eced44d22d752f103f377e7b && \
    make -j ${NCPU:-4} && \
    make install && \
    ldconfig
_EOF_

read_into_variable INSTALL_GOOGLE_CLOUD_CPP_COMMON_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/googleapis/google-cloud-cpp-common/archive/v0.25.0.tar.gz && \
    tar -xf v0.25.0.tar.gz && \
    cd google-cloud-cpp-common-0.25.0 && \
    cmake -H. -Bcmake-out \
        -DBUILD_TESTING=OFF \
        -DGOOGLE_CLOUD_CPP_TESTING_UTIL_ENABLE_INSTALL=ON && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
_EOF_

read_into_variable BUILD_PROJECT_CMAKE_SUPER_FRAGMENT <<'_EOF_'
FROM devtools AS readme
ARG NCPU=4

WORKDIR /home/build/super
COPY . /home/build/super
RUN cmake -Hsuper -Bcmake-out \
        -DGOOGLE_CLOUD_CPP_EXTERNAL_PREFIX=$HOME/local
RUN cmake --build cmake-out -- -j ${NCPU:-4}
_EOF_
