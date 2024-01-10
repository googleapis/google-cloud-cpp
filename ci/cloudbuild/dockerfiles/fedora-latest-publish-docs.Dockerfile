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

FROM fedora:38
ARG NCPU=4
ARG ARCH=amd64

# Install the minimum development tools needed to compile `google-cloud-cpp`.
RUN dnf makecache && \
    dnf install -y clang cmake diffutils findutils gcc-c++ git \
        make ninja-build patch python3 python3-devel \
        python-pip tar unzip which zip

# Then install the development packages for `google-cloud-cpp` dependencies.
RUN dnf makecache && \
    dnf install -y \
        gmock-devel \
        google-benchmark-devel \
        google-crc32c-devel \
        grpc-devel \
        gtest-devel \
        libcurl-devel \
        openssl-devel \
        protobuf-devel \
        pugixml-devel \
        yaml-cpp-devel

# This is used in the `publish-docs` build
RUN dnf makecache && dnf install -y libxslt doxygen

# Sets root's password to the empty string to enable users to get a root shell
# inside the container with `su -` and no password. Sudo would not work because
# we run these containers as the invoking user's uid, which does not exist in
# the container's /etc/passwd file.
RUN echo 'root:' | chpasswd

WORKDIR /var/tmp/build/json
RUN curl -fsSL https://github.com/nlohmann/json/archive/v3.11.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -G Ninja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig

WORKDIR /var/tmp/build/
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.13.0.tar.gz | \
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
        -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.7.5/sccache-v0.7.5-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Update the ld.conf cache in case any libraries were installed in /usr/local/lib*
RUN ldconfig /usr/local/lib*
