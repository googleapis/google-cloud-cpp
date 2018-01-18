# A simple makefile to test the `install` target.
#
# This is not intended to be a demonstration of how to write good Makefiles,
# nor is it a general solution on how to build Makefiles for google-cloud-cpp.
# It is intended to test the installation, many things are hard-coded in this
# test (e.g., PREFIX, the "d" suffix for libraries, the compiler options), which
# should be configured in a production-quality Makefile.
#
# This file should be replaced with a CMakeList.txt file when either of these
# bugs is resolved:
# TODO(#44) - use the generated FindPkgConfig to find the libraries and headers.
# TODO(#45) - use find_package() to find the libraries and headers.

# Assume everything was installed in /usr/local, which it is by default.
PREFIX=/usr/local

CPPFLAGS=-I$(PREFIX)/include -I$(PREFIX)/include/bigtable
CXXLDFLAGS=-L$(PREFIX)/lib -L$(PREFIX)/lib64 # Set both, because why not?
LIBS=-lbigtable_admin_client -lbigtable_client -lbigtable_protos \
    -labsl_strings -lgrpc++ -lgrpc -lprotobuf -lm

# This is hard-coded because it is intended for a test, applications are
# expected to configure the files themselves.
CXX=g++ -std=c++11
CXXLD=$(CXX)

all: install_integration_test

install_integration_test: install_integration_test.cc
	$(CXXLD) $(CPPFLAGS) $(CXXFLAGS) $(CXXLDFLAGS) -o $@ $^ $(LIBS)
