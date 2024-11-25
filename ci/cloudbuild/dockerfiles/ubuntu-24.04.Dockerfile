# Copyright 2024 Google LLC
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

FROM ubuntu:24.04
ARG NCPU=4
ARG ARCH=amd64

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get --no-install-recommends install -y \
        automake \
        build-essential \
        clang \
        cmake \
        curl \
        gawk \
        git \
        gcc \
        g++ \
        cmake \
        libcurl4-openssl-dev \
        libssl-dev \
        libtool \
        llvm \
        lsb-release \
        make \
        ninja-build \
        patch \
        pkg-config \
        python3 \
        python3-dev \
        python3-pip \
        tar \
        unzip \
        zip \
        wget \
        zlib1g-dev \
        apt-utils \
        ca-certificates \
        apt-transport-https

# Install Python packages used in the integration tests.
RUN update-alternatives --install /usr/bin/python python $(which python3) 10
RUN apt update && apt install python3-setuptools python3-wheel

# The Cloud Pub/Sub emulator needs Java :shrug:
RUN apt update && (apt install -y openjdk-11-jre || apt install -y openjdk-9-jre)

# Install the Cloud SDK and some of the emulators. We use the emulators to run
# integration tests for the client libraries.
COPY . /var/tmp/ci
WORKDIR /var/tmp/downloads
RUN /var/tmp/ci/install-cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}

RUN curl -o /usr/bin/bazelisk -sSL "https://github.com/bazelbuild/bazelisk/releases/download/v1.24.1/bazelisk-linux-${ARCH}" && \
    chmod +x /usr/bin/bazelisk && \
    ln -s /usr/bin/bazelisk /usr/bin/bazel
