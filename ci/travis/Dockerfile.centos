# Copyright 2017 Google Inc.
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

ARG DISTRO_VERSION=7
FROM centos:${DISTRO_VERSION}

# We need the "Extra Packages for Enterprise Linux" for cmake3
RUN rpm -Uvh \
    https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm

RUN yum install -y centos-release-scl
RUN yum-config-manager --enable rhel-server-rhscl-7-rpms

RUN yum makecache && yum install -y \
    ccache \
    cmake3 \
    curl-devel \
    gcc \
    gcc-c++ \
    git \
    make \
    openssl-devel \
    python \
    python-pip \
    unzip \
    wget \
    which \
    zlib-devel

RUN pip install --upgrade pip
RUN pip install httpbin flask gevent gunicorn crc32c

# Install cmake3 + ctest3 as cmake + ctest.
RUN ln -sf /usr/bin/cmake3 /usr/bin/cmake && \
    ln -sf /usr/bin/ctest3 /usr/bin/ctest

# Install the Cloud Bigtable emulator and the Cloud Bigtable command-line
# client.  They are used in the integration tests.
COPY . /var/tmp/ci
WORKDIR /var/tmp/downloads
RUN /var/tmp/ci/install-cloud-sdk.sh

# Install Bazel because some of the builds need it.
RUN /var/tmp/ci/install-bazel.sh
