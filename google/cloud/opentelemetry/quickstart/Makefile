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

# This is a minimal Makefile to show how to use the C++ Google Cloud
# OpenTelemetry Exporter library for developers who use make(1) as their build
# system.

# The CXX, CXXFLAGS and CXXLD variables are hard-coded. These values work for
# our tests, but applications would typically make them configurable parameters.
CXX=g++
CXXFLAGS=
CXXLD=$(CXX)
BIN=.

all: $(BIN)/quickstart

# Configuration variables to compile and link against the Google Cloud
# OpenTelemetry Exporter library and the Google Cloud Storage client.
EXAMPLE_MODULES    := google_cloud_cpp_opentelemetry google_cloud_cpp_storage
EXAMPLE_CXXFLAGS   := $(shell pkg-config $(EXAMPLE_MODULES) --cflags)
EXAMPLE_CXXLDFLAGS := $(shell pkg-config $(EXAMPLE_MODULES) --libs-only-L)
EXAMPLE_LIBS       := $(shell pkg-config $(EXAMPLE_MODULES) --libs-only-l)

$(BIN)/quickstart: quickstart.cc
	$(CXXLD) $(CXXFLAGS) $(EXAMPLE_CXXFLAGS) $(EXAMPLE_CXXLDFLAGS) -o $@ $^ $(EXAMPLE_LIBS)
