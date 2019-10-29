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
#         https://github.com/googleapis/google-cloud-cpp-common/ci/templates
#
_EOF_

read_into_variable INSTALL_CPP_CMAKEFILES_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/googleapis/cpp-cmakefiles/archive/v0.1.5.tar.gz
RUN tar -xf v0.1.5.tar.gz
WORKDIR /var/tmp/build/cpp-cmakefiles-0.1.5
RUN cmake \
    -DBUILD_SHARED_LIBS=YES \
    -H. -Bcmake-out
RUN cmake --build cmake-out -- -j ${NCPU:-4}
RUN cmake --build cmake-out --target install -- -j ${NCPU:-4}
RUN ldconfig
_EOF_

read_into_variable INSTALL_GOOGLETEST_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/google/googletest/archive/release-1.10.0.tar.gz
RUN tar -xf release-1.10.0.tar.gz
WORKDIR /var/tmp/build/googletest-release-1.10.0
RUN cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out
RUN cmake --build cmake-out -- -j ${NCPU:-4}
RUN cmake --build cmake-out --target install -- -j ${NCPU:-4}
RUN ldconfig
_EOF_

read_into_variable INSTALL_CRC32C_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
RUN tar -xf 1.0.6.tar.gz
WORKDIR /var/tmp/build/crc32c-1.0.6
RUN cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out
RUN cmake --build cmake-out -- -j ${NCPU:-4}
RUN cmake --build cmake-out --target install -- -j ${NCPU:-4}
RUN ldconfig
_EOF_

read_into_variable INSTALL_PROTOBUF_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/google/protobuf/archive/v3.9.1.tar.gz
RUN tar -xf v3.9.1.tar.gz
WORKDIR /var/tmp/build/protobuf-3.9.1/cmake
RUN cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out
RUN cmake --build cmake-out -- -j ${NCPU:-4}
RUN cmake --build cmake-out --target install -- -j ${NCPU:-4}
RUN ldconfig
_EOF_

read_into_variable INSTALL_C_ARES_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz
RUN tar -xf cares-1_14_0.tar.gz
WORKDIR /var/tmp/build/c-ares-cares-1_14_0
RUN ./buildconf && ./configure && make -j ${NCPU:-4}
RUN make install
RUN ldconfig
_EOF_

read_into_variable INSTALL_GRPC_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/grpc/grpc/archive/v1.23.1.tar.gz
RUN tar -xf v1.23.1.tar.gz
WORKDIR /var/tmp/build/grpc-1.23.1
RUN make -j ${NCPU:-4}
RUN make install
RUN ldconfig
_EOF_

read_into_variable INSTALL_GOOGLE_CLOUD_CPP_COMMON_FROM_SOURCE <<'_EOF_'
WORKDIR /var/tmp/build
RUN wget -q https://github.com/googleapis/google-cloud-cpp-common/archive/v0.13.0.tar.gz
RUN tar -xf v0.13.0.tar.gz
WORKDIR /var/tmp/build/google-cloud-cpp-common-0.13.0
RUN cmake -H. -Bcmake-out \
    -DBUILD_TESTING=OFF \
    -DGOOGLE_CLOUD_CPP_TESTING_UTIL_ENABLE_INSTALL=ON
RUN cmake --build cmake-out -- -j ${NCPU:-4}
RUN cmake --build cmake-out --target install -- -j ${NCPU:-4}
RUN ldconfig
_EOF_

read_into_variable BUILD_PROJECT_CMAKE_SUPER_FRAGMENT <<'_EOF_'
WORKDIR /home/build/super
COPY . /home/build/super
RUN cmake -Hsuper -Bcmake-out \
        -DGOOGLE_CLOUD_CPP_EXTERNAL_PREFIX=$HOME/local
RUN cmake --build cmake-out -- -j ${NCPU}
_EOF_
