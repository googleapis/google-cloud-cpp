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

FROM ubuntu:bionic AS devtools

# Install the typical development tools and some additions:
RUN apt update && \
    apt install -y build-essential cmake git gcc g++ cmake \
        libssl-dev make pkg-config tar wget zlib1g-dev

WORKDIR /var/tmp/build
RUN wget -q https://github.com/curl/curl/archive/curl-7_64_0.tar.gz
RUN tar -xf curl-7_64_0.tar.gz
WORKDIR /var/tmp/build/curl-curl-7_64_0
RUN ls -l
RUN cmake -H. -B.build \
        -DHTTP_ONLY=ON \
        -DCMAKE_USE_OPENSSL=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DCURL_STATICLIB=ON
RUN cmake --build .build --target install -- -j $(nproc)

FROM devtools AS build

# Copy the source code to /w, and then compile the program(s) we need.
WORKDIR /w
COPY . /w

ARG CMAKE_BUILD_TYPE=Release
ARG SHORT_SHA=""

RUN cmake -H. -B.build \
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
    -DGOOGLE_CLOUD_CPP_BUILD_METADATA=${SHORT_SHA}

RUN cmake \
    --build .build \
    --target read_object_stall_test \
    -- -j $(nproc)

RUN cmake \
    --build .build \
    --target storage_object_samples \
    -- -j $(nproc)

# ================================================================

# Prepare the final image, this image is much smaller because only the required
# dependencies are installed.
FROM ubuntu:bionic
RUN apt update && \
    apt install -y libstdc++6 zlib1g binutils ca-certificates \
        tcpdump coreutils strace procps
RUN /usr/sbin/update-ca-certificates

COPY --from=build \
    /w/.build/google/cloud/storage/tests/read_object_stall_regression/read_object_stall_test \
    /w/.build/google/cloud/storage/examples/storage_object_samples \
    /r/

CMD ["/bin/false"]
