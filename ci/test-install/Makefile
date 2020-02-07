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

# A simple Makefile to test the `install` target.
#
# This is not intended to be a demonstration of how to write good Makefiles,
# nor is it a general solution on how to build Makefiles for google-cloud-cpp.
# It is simply a minimal file to test the installed pkg-config support files.

# The CXX, CXXFLAGS and CXXLD variables are hard-coded. These values work for
# our tests, but applications would typically make them configurable parameters.
CXX=g++
CXXFLAGS=-std=c++11
CXXLD=$(CXX)

all: bigtable_install_test storage_install_test

# Configuration variables to compile and link against the Cloud Bigtable C++
# client library.
CBT_DEPS := bigtable_client
CBT_CXXFLAGS   := $(shell pkg-config $(CBT_DEPS) --cflags)
CBT_CXXLDFLAGS := $(shell pkg-config $(CBT_DEPS) --libs-only-L)
CBT_LIBS       := $(shell pkg-config $(CBT_DEPS) --libs-only-l)

# A target using the Cloud Bigtable C++ client library.
bigtable_install_test: bigtable/bigtable_install_test.cc
	$(CXXLD) $(CXXFLAGS) $(CBT_CXXFLAGS) $(GCS_CXXLDFLAGS) -o $@ $^ $(CBT_LIBS)


# Configuration variables to compile and link against the Google Cloud Storage
# C++ client library.
GCS_DEPS := storage_client
GCS_CXXFLAGS   := $(shell pkg-config $(GCS_DEPS) --cflags)
GCS_CXXLDFLAGS := $(shell pkg-config $(GCS_DEPS) --libs-only-L)
GCS_LIBS       := $(shell pkg-config $(GCS_DEPS) --libs-only-l)

# A target using the Google Cloud Storage C++ client library.
storage_install_test: storage/storage_install_test.cc
	$(CXXLD) $(CXXFLAGS) $(GCS_CXXFLAGS) $(GCS_CXXLDFLAGS) -o $@ $^ $(GCS_LIBS)
