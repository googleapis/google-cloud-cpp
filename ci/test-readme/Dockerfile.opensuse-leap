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

ARG DISTRO_VERSION=latest
FROM opensuse/leap:${DISTRO_VERSION} AS devtools

## [START INSTALL.md]

# Install the minimal development tools:

## [START README.md]

# ```bash
RUN zypper refresh && \
    zypper install --allow-downgrade -y cmake gcc gcc-c++ git gzip \
        libcurl-devel libopenssl-devel make tar wget
# ```

## [END README.md]

FROM devtools
COPY ci/install-retry.sh /retry3

## [START IGNORED]
# Verify that the tools above are enough to compile google-cloud-cpp when using
# external projects.
WORKDIR /home/build/external
COPY . /home/build/external
RUN cmake -H. -Bcmake-out -DCMAKE_BUILD_TYPE=Debug
RUN cmake --build cmake-out -- -j $(nproc)
RUN (cd cmake-out && ctest --output-on-failure)
## [END IGNORED]

# #### crc32c

# There is no OpenSUSE package for this library. To install it, use:

# ```bash
WORKDIR /var/tmp/build
RUN /retry3 wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
RUN tar -xf 1.0.6.tar.gz
WORKDIR /var/tmp/build/crc32c-1.0.6
RUN cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out/crc32c
RUN cmake --build cmake-out/crc32c --target install -- -j $(nproc)
RUN ldconfig
# ```

# #### Protobuf

# OpenSUSE Leap includes a package for protobuf-2.6, but this is too old to
# support the Google Cloud Platform proto files, or to support gRPC for that
# matter. Manually install protobuf:

# ```bash
WORKDIR /var/tmp/build
RUN /retry3 wget -q https://github.com/google/protobuf/archive/v3.6.1.tar.gz
RUN tar -xf v3.6.1.tar.gz
WORKDIR /var/tmp/build/protobuf-3.6.1/cmake
RUN cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out
RUN cmake --build cmake-out --target install -- -j $(nproc)
RUN ldconfig
# ```

# #### c-ares

# Recent versions of gRPC require c-ares >= 1.11, while OpenSUSE Leap
# distributes c-ares-1.9. We need some additional development tools to compile
# this library:

# ```bash
RUN /retry3 zypper refresh && \
    /retry3 zypper install -y automake libtool
# ```

# Manually install a newer version:

# ```bash
WORKDIR /var/tmp/build
RUN /retry3 wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz
RUN tar -xf cares-1_14_0.tar.gz
WORKDIR /var/tmp/build/c-ares-cares-1_14_0
RUN ./buildconf && ./configure && make -j $(nproc)
RUN make install
RUN ldconfig
# ```

# #### gRPC

# The gRPC Makefile uses `which` to determine whether the compiler is available.
# Install this command for the extremely rare case where it may be missing from
# your workstation or build server:

# ```bash
RUN /retry3 zypper refresh && \
    /retry3 zypper install -y which
# ```

# Then gRPC can be manually installed using:

# ```bash
WORKDIR /var/tmp/build
RUN /retry3 wget -q https://github.com/grpc/grpc/archive/v1.19.1.tar.gz
RUN tar -xf v1.19.1.tar.gz
WORKDIR /var/tmp/build/grpc-1.19.1
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
ENV PATH=/usr/local/bin:${PATH}
RUN make -j $(nproc)
RUN make install
RUN ldconfig
# ```

# #### google-cloud-cpp

# We can now compile and install `google-cloud-cpp`.

# ```bash
WORKDIR /home/build/google-cloud-cpp
COPY . /home/build/google-cloud-cpp
RUN cmake -H. -Bcmake-out \
    -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package \
    -DGOOGLE_CLOUD_CPP_GMOCK_PROVIDER=external
RUN cmake --build cmake-out -- -j $(nproc)
WORKDIR /home/build/google-cloud-cpp/cmake-out
RUN ctest --output-on-failure
RUN cmake --build . --target install
# ```

## [END INSTALL.md]

# Verify that the installed files are actually usable
WORKDIR /home/build/test-install-plain-make
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig
COPY ci/test-install /home/build/test-install-plain-make
RUN make

WORKDIR /home/build/test-install-cmake-bigtable
COPY ci/test-install/bigtable /home/build/test-install-cmake-bigtable
RUN env -u PKG_CONFIG_PATH cmake -H. -Bcmake-out
RUN cmake --build cmake-out -- -j $(nproc)

WORKDIR /home/build/test-install-cmake-storage
COPY ci/test-install/storage /home/build/test-install-cmake-storage
RUN env -u PKG_CONFIG_PATH cmake -H. -Bcmake-out
RUN cmake --build cmake-out -- -j $(nproc)

WORKDIR /home/build/test-submodule
COPY ci/test-install /home/build/test-submodule
COPY . /home/build/test-submodule/submodule/google-cloud-cpp
RUN cmake -Hsubmodule -Bcmake-out
RUN cmake --build cmake-out -- -j $(nproc)
