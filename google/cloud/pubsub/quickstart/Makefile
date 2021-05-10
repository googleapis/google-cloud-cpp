# Copyright 2020 Google LLC
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

# This is a minimal Makefile to show how to use the Cloud Pub/Sub C++ client for
# developers that use make(1) as their build system.

# The CXX, CXXFLAGS and CXXLD variables are hard-coded. These values work for
# our tests, but applications would typically make them configurable parameters.
CXX=g++
CXXFLAGS=-std=c++11
CXXLD=$(CXX)
BIN=.

all: $(BIN)/quickstart

# Configuration variables to compile and link against the Cloud Spanner C++
# client library.
PUBSUB_DEPS := pubsub_client
PUBSUB_CXXFLAGS   := $(shell pkg-config $(PUBSUB_DEPS) --cflags)
PUBSUB_CXXLDFLAGS := $(shell pkg-config $(PUBSUB_DEPS) --libs-only-L)
PUBSUB_LIBS       := $(shell pkg-config $(PUBSUB_DEPS) --libs-only-l)

# A target using the Cloud Pubsub C++ client library.
$(BIN)/quickstart: quickstart.cc
	$(CXXLD) $(CXXFLAGS) $(PUBSUB_CXXFLAGS) $(PUBSUB_CXXLDFLAGS) -o $@ $^ $(PUBSUB_LIBS)
