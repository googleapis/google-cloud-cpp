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

DEPS=bigtable_client grpc++ grpc protobuf
CXXFLAGS=$(shell pkg-config $(DEPS) --cflags)
CXXLDFLAGS=$(shell pkg-config $(DEPS) --libs-only-L)
LIBS=$(shell pkg-config $(DEPS) --libs-only-l)

# This is hard-coded because it is intended for a test, applications are
# expected to configure the files themselves.
CXX=clang++ -std=c++11
CXXLD=$(CXX)

all: echo install_integration_test

install_integration_test: install_integration_test.cc
	$(CXXLD) $(CXXFLAGS) $(CXXLDFLAGS) -o $@ $^ $(LIBS)

echo:
	echo $(DEPS)
