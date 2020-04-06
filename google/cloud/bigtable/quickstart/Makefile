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

# This is a minimal Makefile to show how to use the Cloud Bigtable C++ client
# for developers that use make(1) as their build system.

# The CXX, CXXFLAGS and CXXLD variables are hard-coded. These values work for
# our tests, but applications would typically make them configurable parameters.
CXX=g++
CXXFLAGS=-std=c++11
CXXLD=$(CXX)

all: quickstart

# Configuration variables to compile and link against the Cloud Bigtable C++
# client library.
CBT_DEPS := bigtable_client
CBT_CXXFLAGS   := $(shell pkg-config $(CBT_DEPS) --cflags)
CBT_CXXLDFLAGS := $(shell pkg-config $(CBT_DEPS) --libs-only-L)
CBT_LIBS       := $(shell pkg-config $(CBT_DEPS) --libs-only-l)

# A target using the Cloud Bigtable C++ client library.
quickstart: quickstart.cc
	$(CXXLD) $(CXXFLAGS) $(CBT_CXXFLAGS) $(GCS_CXXLDFLAGS) -o $@ $^ $(CBT_LIBS)
