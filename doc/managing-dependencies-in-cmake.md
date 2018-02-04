# Dependency Management for google-cloud-cpp

`google-cloud-cpp` must have a minimal set of dependencies, every dependency is
viewed as a blocker for adoption by our customers.  Only the following
libraries are allowed as dependencies:

* [googleapis](https://github.com/google/googleapis) because it contains the
protocol definition (as protobufs) to contact Google Cloud services.
* [gRPC](https://grpc.io) because this implements the protocol used to contact
Google Cloud services.
* [abseil-cpp](https://abseil.io) because it offers useful functionality and it
is expected to become a dependency on protobuf and gRPC.
* [googletest](https://github.com/google/googletest) because we needed a unit
test framework and this was already a dependency for googleapis, gRPC, and
Abseil.

Some of these libraries have dependencies themselves, for example, gRPC depends
on [protobuf](https://developers.google.com/protocol-buffers/), Abseil depends
on [cctz](https://github.com/google/cctz).

This document describes how these dependencies are included in CMake files,
and how they can be used by applications or libraries that depend on
`google-cloud-cpp`.

## Overview

When compiling from source, we use dependencies as git submodules. Dependency
submodules are included using the `add_subdirectory()` macro.  We use the
`EXCLUDE_FROM_ALL` parameter to avoid compiling additional targets, such as
examples or tests, that are not required for the `google-cloud-cpp` targets.
Submodules included with `add_subdirectory()` introduce targets (such as
libraries) that we can add as dependencies to the libraries and executables
defined in `google-cloud-cpp`. Any compiler flags, such as include directories,
needed for these targets are automatically added when the target becomes a
dependency.

We want installed dependencies to have the same behavior. CMake supports this
use case well as long as projects follow the following conventions:

* One should use dependencies via their `EXPORTED` name, for example,
`absl::base` and not `absl_base`.
* Dependencies should use `target_include_directories()`
and `target_compile_definitions()` to add compilation flags.
* Libraries should install a package config file when they are installed.

In this document we describe how the code in `google-cloud-cpp` can be used
without change between installed and submodule versions.

## Detailed Management for all Dependencies

In this section we analyse the existing dependencies and how do we propose to
manage them.

### Abseil

Even when included as a submodule Abseil is hard to manage.  It has a strong
dependency on `cctz` and `googletest`, going as far as checking that the targets
for each exist.  However, it does not provide any of the targets for them. The
library assumes that it is being compiled as a submodule that provides these
dependencies.

This is problematic because gRPC creates the targets for googletest when used as
a submodule, but does not provide a `gtest` target when already installed.
We propose to fix this problem by:
 
* Providing the `cctz` and `googletest` targets when Abseil is a submodule.
* Requiring that Abseil and gRPC are either both used as submodules or both
used as packages.

Furthermore, Abseil does not provide targets to install it. It recommends
that applications always compile from source. We believe this is unrealistic
for all our customers. We will support compiling against a installed version
of Abseil, as long as the application provides (a) either support files for
`find_package()` in CMake, or (b) support files for `pkg-config`.

Whether the application creates these files by themselves, they use a forked
version of Abseil that has created these files, or they convince the Abseil
developers to create these files themselves is outside the scope of this
project.

### gRPC

gRPC can be compiled using CMake, plain GNU Make, Bazel, or vcpkg.  Depending
how it is compiled it may offer an install target (GNU Make and CMake have
them) and it may or may not have installed config files for `find_package()`.

We propose to support three different modes for gRPC:

1. As a installed package with CMake support.  In this case we expect gRPC to
*not* create any of the `googletest` targets.  The exported names are prefixed
by `gRPC` in this case, e.g., `gRPC::grpc++`, `gRPC::grpc`.

1. As a installed package with `pkg-config` support.  In this case we will
use the output from `pkg-config` to create `IMPORTED` targets with the same
names (e.g., `gRPC::grpc++`) as the targets created by an installed gRPC library
with CMake support.

1. As a submodule.  In this case gRPC creates a `gtest` and `gtest_main` target,
but not `gmock` target.  It also does not create the exported names for its
own libraries.  We will create the `gRPC::*` aliases to mimic an installed
library.

#### Sub-Dependencies of gRPC

gRPC has many dependencies.  When this document was written, they include:
[abseil-cpp](https://abseil.io),
[benchmark](https://github.com/google/benchmark),
[bloaty](https://github.com/google/bloaty), 
[boringssl](https://github.com/google/boringssl),
[boring-ssl-with-bazel](https://github.com/google/boringssl/tree/master-with-bazel),
[c-ares](https://github.com/c-ares/c-ares),
[gflags](https://github.com/gflags/gflags),
[googletest](https://github.com/google/googletest),
[nanopb](https://github.com/nanopb/nanopb),
[protobuf](https://developers.google.com/protocol-buffers/),
[zlib](https://www.zlib.net/), as well as the standard system library
dependencies such as `libpthread`, `libc` and the C++ library (typically
`libstdc++`). Of all these dependencies, only `protobuf`, `zlib`, and `pthread`
need to be explicitly linked by the application.

We propose to manage `protobuf` and `zlib` just as we manage `gRPC`: when
installed with CMake support we will use that support, when used as a submodule
or when installed with only `pkg-config` support we create imported or aliased
targets that mimic the installed CMake support, i.e., the target is always
`Protobuf::libprotobuf`.

`libpthread` has native support in CMake and we will simply use that: the
`FindThreads` module creates a `Threads::Threads` target that works on all
platforms.

### Googleapis

The `third_party/googleapis` submodule contains the proto files for all Google
APIs, most of which are not used by `google-cloud-cpp`.  While `googleapis`
includes a `Makefile` for Unix-like platforms, this only creates the
proto-generated files.  It does not compile the generated files, does not create
any libraries, it is not portable to other platforms, and does not provide any
mechanism to install artifacts.  For the time being we will use this dependency
**only** as a submodule, and compile  a library with the minimal set of protos
needed by `google-cloud-cpp`.

Every time a new service is wrapper in `google-cloud-cpp` is updated this
library will need to be updated too.

### Googletest

Googletest is not installed, nor should it be, and therefore it is only used
as a submodule.

## Implementation

`google-cloud-cpp` defines a `*_PROVIDER` macro to control the support for each
dependency. If the macro is defined as `module`, then that target is used as
a submodule. If the macro is defined as `package`, then that target is used as
a installed package with CMake support. If the macro is defined as `pkg-config`,
the dependency is assumed to be installed with only `pkg-config` support.

We will create a separate file in `cmake/Include<Dependency>.cmake` for each of
the dependencies. The file will typically be of this form:

```cmake
set(GOOGLE_CLOUD_CPP_GRPC_PROVIDER "module" CACHE STRING "How to find the gRPC library")
set_property(CACHE GOOGLE_CLOUD_CPP_GRPC_PROVIDER PROPERTY STRINGS "module" "package" "pkg-config")
if ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "module")
    # Some checking
    add_subdirectory(third_party/grpc EXCLUDE_FROM_ALL)
    # Define aliases if needed.
    add_library(gRPC::grpc++ ALIAS grpc++)
    # ... some code ommitted ...
elseif ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "package")
    find_package(GRPCPP REQUIRED >= 1.8)
    find_package(GRPC REQUIRED)
else ()
    # Find the packages using `pkg-config`:
    include(FindPkgConfig)
    pkg_check_modules(gRPCPP REQUIRED IMPORTED_TARGET grpc++ >= 1.9.0)
    pkg_check_modules(gRPC REQUIRED IMPORTED_TARGET )
    # Define the target as an imported target, such targets can have properties
    # but they cannot be compiled:
    add_library(gRPC::grpc++ ALIAS PkgConfig::gRPCPP)
    add_library(gRPC::grpc ALIAS PkgConfig::gRPC)
endif ()
```

## Testing

The configurations must be tested as part of the CI builds. To ensure coverage
we will:

* Most builds will compile the dependencies from source, as submodules. This is
how the `google-cloud-cpp` developers use the system and we expect that many of
the users will too.

* We will dedicate one CI build on Linux to compile against installed
dependencies using `pkg-config` files.

* On Windows, we already use installed dependencies and CMake support files. On
this platform compiling from source can be (a) so slow that we go over the 
time allocated in the CI build, and (b) the build may require patching the
dependencies, as the `vcpkg` ports do.  Since `vcpkg` already takes care of
building the dependencies and creating the CMake support files we might as well
use it to validate this build type.
