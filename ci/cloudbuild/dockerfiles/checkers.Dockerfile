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

# This Dockerfile only contains the tools needed to run the `checkers.sh`
# build. The specific Linux distro and version don't really matter much, other
# than to the extent that certain distros offer certain versions of software
# that the build needs. It's fine to add more deps that are needed by the
# `checkers.sh` build.
FROM fedora:34
ARG NCPU=4

RUN dnf makecache && \
    dnf install -y \
        cargo \
        clang-tools-extra \
        diffutils \
        findutils \
        git \
        python-pip \
        ShellCheck

RUN curl -L -o /usr/bin/buildifier https://github.com/bazelbuild/buildtools/releases/download/0.29.0/buildifier && \
    chmod 755 /usr/bin/buildifier

RUN curl -L -o /usr/local/bin/shfmt https://github.com/mvdan/sh/releases/download/v3.1.0/shfmt_v3.1.0_linux_amd64 && \
    chmod 755 /usr/local/bin/shfmt

RUN pip3 install --upgrade pip
RUN pip3 install cmake_format==0.6.8
RUN pip3 install black==19.3b0

RUN cargo install typos-cli --version 1.0.3 --root /usr/local
