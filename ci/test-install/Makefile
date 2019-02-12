# Copyright 2018 Google Inc.
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

# A simple makefile to test the `install` target.
#
# This is not intended to be a demonstration of how to write good Makefiles,
# nor is it a general solution on how to build Makefiles for google-cloud-cpp.
# It is simply a minimal file to test the installed pkg-config support files.

# This is hard-coded because it is intended for a test, applications typically
# have their own configuration files.
CXX=g++
CXXFLAGS=-std=c++11
CXXLD=$(CXX)


all: bigtable_install_test storage_install_test

CBT_DEPS := bigtable_client google_cloud_cpp_common grpc++ grpc zlib openssl libcares protobuf
CBT_CXXFLAGS   := $(shell pkg-config $(CBT_DEPS) --cflags)
CBT_CXXLDFLAGS := $(shell pkg-config $(CBT_DEPS) --libs-only-L)
# gRPC does not install a full complement of pkg-config files:
#   https://github.com/grpc/grpc/issues/15166
# so we need to "guess" that -lgpr and -laddress_sorting will be needed.
CBT_LIBS := \
    $(shell pkg-config $(CBT_DEPS) --libs-only-l) \
    -lgpr -laddress_sorting \
    $(shell pkg-config libcares --libs-only-l)

bigtable_install_test: bigtable_install_test.cc
	$(CXXLD) $(CXXFLAGS) $(CBT_CXXFLAGS) $(GCS_CXXLDFLAGS) -o $@ $^ $(CBT_LIBS)


GCS_DEPS := storage_client google_cloud_cpp_common libcurl openssl
GCS_CXXFLAGS   := $(shell pkg-config $(GCS_DEPS) --cflags)
GCS_CXXLDFLAGS := $(shell pkg-config $(GCS_DEPS) --libs-only-L)
# CRC32C does not have pkg-config(1) files, so link it explicitly.
GCS_LIBS := $(shell pkg-config $(GCS_DEPS) --libs-only-l) -lcrc32c

storage_install_test: storage_install_test.cc
	$(CXXLD) $(CXXFLAGS) $(GCS_CXXFLAGS) $(GCS_CXXLDFLAGS) -o $@ $^ $(GCS_LIBS)
