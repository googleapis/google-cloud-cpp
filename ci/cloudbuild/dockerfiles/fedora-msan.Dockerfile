# Copyright 2021 Google LLC
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

# Install the minimal packages needed to compile libcxx, install Bazel, and
# then compile our code.
RUN dnf makecache && \
    dnf install -y clang clang-tools-extra cmake findutils \
        git llvm make ninja-build openssl-devel patch python \
        python3 python3-devel python3-lit python-pip tar unzip which wget xz

# Install the Python modules needed to run the storage emulator
RUN dnf makecache && dnf install -y python3-devel
RUN pip3 install --upgrade pip
RUN pip3 install setuptools wheel

# The Cloud Pub/Sub emulator needs Java :shrug:
RUN dnf makecache && dnf install -y java-latest-openjdk-devel

# Sets root's password to the empty string to enable users to get a root shell
# inside the container with `su -` and no password. Sudo would not work because
# we run these containers as the invoking user's uid, which does not exist in
# the container's /etc/passwd file.
RUN echo 'root:cloudcxx' | chpasswd

WORKDIR /var/tmp/build

# Install instructions from:
#     https://github.com/google/sanitizers/wiki/MemorySanitizerLibcxxHowTo
# with updates from:
#     https://github.com/google/sanitizers/issues/1685
#
# Starting with 18.0 libcxx defaults to using LLVM's unwind library:
#     https://libcxx.llvm.org/ReleaseNotes/18.html#build-system-changes
# This does not work for us (not sure why exactly), we need to use the system
# library, and therefore turn off the LLVM_UNWINDER.
#
RUN git clone --depth=1 --branch llvmorg-18.1.1 https://github.com/llvm/llvm-project
WORKDIR /var/tmp/build/llvm-project
# configure cmake
RUN cmake -GNinja -S runtimes -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_USE_SANITIZER=MemoryWithOrigins \
    -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
    -DCOMPILER_RT_USE_LLVM_UNWINDER=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr
# build the libraries
RUN cmake --build build
RUN cmake --install build
# The Fedora package for LLVM does something similar:
#     https://src.fedoraproject.org/rpms/llvm/blob/rawhide/f/llvm.spec#_374
RUN echo "/usr/lib/x86_64-unknown-linux-gnu" >/etc/ld.so.conf.d/msan.conf
RUN ldconfig

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

RUN curl -o /usr/bin/bazelisk -sSL "https://github.com/bazelbuild/bazelisk/releases/download/v1.22.0/bazelisk-linux-${ARCH}" && \
    chmod +x /usr/bin/bazelisk && \
    ln -s /usr/bin/bazelisk /usr/bin/bazel
