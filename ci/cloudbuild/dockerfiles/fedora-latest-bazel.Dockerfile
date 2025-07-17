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

FROM fedora:40
ARG NCPU=4
ARG ARCH=amd64

# Install the minimal packages needed to install Bazel, and then compile our
# code.
RUN dnf install -y clang diffutils findutils gcc-c++ git jq lcov libcxx-devel \
        libcxxabi-devel libasan libubsan libtsan llvm patch python python3 \
        python-pip tar unzip w3m wget which zip zlib-devel

# Install the Python modules needed to run the storage emulator
RUN dnf makecache && dnf install -y python3-devel
RUN pip3 install --upgrade pip
RUN pip3 install setuptools wheel

# The Cloud Pub/Sub emulator needs Java, and so does `bazel coverage` :shrug:
# Bazel needs the '-devel' version with javac.
RUN dnf makecache && dnf install -y java-latest-openjdk-devel

# Sets root's password to the empty string to enable users to get a root shell
# inside the container with `su -` and no password. Sudo would not work because
# we run these containers as the invoking user's uid, which does not exist in
# the container's /etc/passwd file.
RUN echo 'root:cloudcxx' | chpasswd

WORKDIR /var/tmp/build

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

RUN curl -o /usr/bin/bazelisk -sSL "https://github.com/bazelbuild/bazelisk/releases/download/v1.24.1/bazelisk-linux-${ARCH}" && \
    chmod +x /usr/bin/bazelisk && \
    ln -s /usr/bin/bazelisk /usr/bin/bazel

# Download the packages needed to run Bigtable conformance tests.
WORKDIR /var/tmp/downloads
RUN wget -O go.tgz https://go.dev/dl/go1.20.5.linux-amd64.tar.gz
RUN tar -C /usr/local/ -xzf go.tgz
ENV GO_LOCATION=/usr/local/go
ENV PATH=${GO_LOCATION}/bin:${PATH}
RUN go version
WORKDIR /var/tmp/downloads/cloud-bigtable-clients-test
RUN curl -fsSL https://github.com/googleapis/cloud-bigtable-clients-test/archive/v0.0.4.tar.gz | \
    tar -xzf - --strip-components=1
