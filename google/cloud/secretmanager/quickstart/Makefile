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

# This is a minimal Makefile to show how to use the Secret Manager API C++ client
# for developers who use make(1) as their build system.

# The CXX, CXXFLAGS and CXXLD variables are hard-coded. These values work for
# our tests, but applications would typically make them configurable parameters.
CXX=g++
CXXFLAGS=
CXXLD=$(CXX)
BIN=.

all: $(BIN)/quickstart

# Configuration variables to compile and link against the Secret Manager API C++
# client library.
CLIENT_MODULE     := google_cloud_cpp_secretmanager
CLIENT_CXXFLAGS   := $(shell pkg-config $(CLIENT_MODULE) --cflags)
CLIENT_CXXLDFLAGS := $(shell pkg-config $(CLIENT_MODULE) --libs-only-L)
CLIENT_LIBS       := $(shell pkg-config $(CLIENT_MODULE) --libs-only-l)

$(BIN)/quickstart: quickstart.cc
	$(CXXLD) $(CXXFLAGS) $(CLIENT_CXXFLAGS) $(CLIENT_CXXLDFLAGS) -o $@ $^ $(CLIENT_LIBS)
