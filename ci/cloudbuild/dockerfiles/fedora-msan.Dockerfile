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

ARG DISTRO_VERSION=33
FROM fedora:${DISTRO_VERSION}
ARG NCPU=4

# Install the minimal packages needed to compile libcxx, install Bazel, and
# then compile our code.
RUN dnf makecache && \
    dnf install -y ccache clang clang-tools-extra cmake findutils gcc-c++ \
        git llvm llvm-devel make ninja-build openssl-devel patch python \
        python3.8 python3-devel python3-lit python-pip tar unzip which wget xz

# Sets root's password to the empty string to enable users to get a root shell
# inside the container with `su -` and no password. Sudo would not work because
# we run these containers as the invoking user's uid, which does not exist in
# the container's /etc/passwd file.
RUN echo 'root:' | chpasswd

# Install the Python modules needed to run the storage emulator
RUN pip3 install --upgrade pip
RUN pip3 install setuptools wheel
RUN pip3 install git+git://github.com/googleapis/python-storage@8cf6c62a96ba3fff7e5028d931231e28e5029f1c
RUN pip3 install flask==1.1.2 httpbin==0.7.0 scalpl==0.4.0 \
    crc32c==2.1 gunicorn==20.0.4

WORKDIR /var/tmp/build

# Install instructions from:
# https://github.com/google/sanitizers/wiki/MemorySanitizerLibcxxHowTo
RUN git clone --depth=1 --branch llvmorg-11.0.0 https://github.com/llvm/llvm-project
WORKDIR llvm-project/build
# configure cmake
RUN cmake -GNinja ../llvm \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="libcxx;libcxxabi" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_USE_SANITIZER=MemoryWithOrigins \
    -DCMAKE_INSTALL_PREFIX=/usr
# build the libraries
RUN cmake --build . -- cxx cxxabi
RUN cmake --build . -- install-cxx install-cxxabi

# Install the Cloud SDK and some of the emulators. We use the emulators to run
# integration tests for the client libraries.
COPY . /var/tmp/ci
WORKDIR /var/tmp/downloads
ENV CLOUDSDK_PYTHON=python3.8
RUN /var/tmp/ci/install-cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}
# The Cloud Pub/Sub emulator needs Java :shrug:
RUN dnf makecache && dnf install -y java-latest-openjdk

# We need Bazel for this build.
COPY . /var/tmp/ci
RUN /var/tmp/ci/install-bazel.sh
