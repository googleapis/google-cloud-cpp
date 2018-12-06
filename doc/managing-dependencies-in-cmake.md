# Dependency Management for google-cloud-cpp

`google-cloud-cpp` must have a minimal set of dependencies, every dependency is
viewed as a blocker for adoption by our customers.  Only the following
libraries are allowed as dependencies:

* [googleapis](https://github.com/google/googleapis) because it contains the
  protocol definition (as protobufs) to contact Google Cloud services.
* [gRPC](https://grpc.io) because this implements the protocol used to contact
  Google Cloud services.
* [googletest](https://github.com/google/googletest) because we needed a unit
  test framework and this was already a dependency for googleapis, gRPC, and
  Abseil.
* [libcurl](https://github.com/curl/curl) is used by the storage library to send
  http requests.
* [nlohmann/json](https://github.com/nlohmann/json) is used by the storage
  library to parse and generate JSON objects.
* [OpenSSL](https://www.openssl.org/source/) is used by the storage library to
  perform some Base64 encoding and JWT signing.

Some of these libraries have dependencies themselves:

* [protobuf](https://developers.google.com/protocol-buffers/) is a dependency
  for gRPC.
* [zlib](https://github.com/madler/zlib) is a dependency for protobuf.
* [c-ares](https://c-ares.haxx.se/) is a dependency for gRPC.
* One of the forks of [OpenSSL](https://www.openssl.org/source/), such as
  [BoringSSL](https://github.com/google/boringssl) or
  [LibreSSL](https://www.libressl.org/) is a dependency for both gRPC and
  libcurl. 

This document describes how the direct dependencies of `google-cloud-cpp`
included in CMake files.

## Overview

We will support two different modes to compile the library with CMake:

- Using [CMake external projects][cmake-doc-externalproject]
- Using pre-installed dependencies.

Bazel builds are basically external project builds.

When compiline with external projects we will create
[targets][cmake-doc-targets] that have the same names (and roles) as the
installed for that dependency.
For example, when protobuf is installed and discovered via `find_package()` the
module for protobuf introduces a `protobuf::libprotobuf` target. We will
construct our external projects to exhibit the same behavior.

CMake supports this use case well as long as projects follow the following
conventions:

* One should use dependencies via their [exported][cmake-doc-export] names, for
  example, `absl::base` and not `absl_base`.
* Dependencies should use `target_include_directories()` and
  `target_compile_definitions()` to add compilation flags.
* Dependencies should create a [package][cmake-doc-packages] config file as part
  of their installation.

## Detailed Management for all Dependencies

In this section we analyse the existing dependencies and how do we propose to
manage them.

### gRPC and Protobuf

gRPC can be compiled using CMake, plain GNU Make, Bazel, or vcpkg.  Depending
how it is compiled it may offer an install target (GNU Make and CMake have
them), and it may or may not have installed config files for `find_package()`.

We will support four different configurations for gRPC:

1. [`vcpkg`](https://github.com/Microsoft/vcpkg) packages gRPC, and provides
   targets (`gRPC::gprc++` and `gRPC::grpc`) for CMake.  
   When `GOOGLE_CLOUD_CPP_GRPC_PROVIDER` is set to `vcpkg` we will enable
   these targets using `find_package()`.  We will also use `find_package()` to
   find the `protobuf` library.  The package introduces a
   `protobuf::libprotobuf` target.

1. [`pkg-config`](https://www.freedesktop.org/wiki/Software/pkg-config/) when
   compiled and installed with GNU Make gRPC installs `pkg-config` support
   files. When `GOOGLE_CLOUD_CPP_GRPC_PROVIDER` is set to `pkg-config` we will
   define `INTERFACE` libraries for `gRPC::grpc++` and `gRPC::grpc`each `absl::*`
   library used in `google-cloud-cpp`.  These `INTERFACE` libraries will set
   their target [properties][cmake-doc-target-properties] based on the
   configuration flags discovered via `pkg-config`. We will also use
   `pkg-config` to find `protobuf` in this case, and introduce
   `protobuf::libprotobuf` as an `INTERFACE` target with its properties set
   based on the `pkg-config` parameters. Note that on CMake-3.6 and higher the
   `Protobuf` CMake module automatically introduces `protobuf::libprotobuf`. We
   are targeting CMake-3.5, which does not offer this feature.
   
1. `package`: When `GOOGLE_CLOUD_CPP_GRPC_PROVIDER` is set to `package` we
    will use `find_package(... grpc)` to find the gRPC libraries. Note that gRPC
    installs the necessary CMake support files when compiled with CMake, but not
    when compiled and installed with GNU Make, so we cannot assume these support
    files always exist.
    
1. `external`: When `GOOGLE_CLOUD_CPP_GRPC_PROVIDER` is set to `module`
   (the default) we will simply add the `third_party/grpc` subdirectory to
   the CMake build.

#### Things that gRPC depends on

gRPC has many submodules.  When this document was written, they include:
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

We already discussed how `protobuf` is discovered alongside `gRPC`.

`zlib` is often installed as a system library and has a relatively stable
interface.  Unless it is used as a module we will use the native library via
`find_package`.

`libpthread` has native support in CMake and we will simply use that: the
`FindThreads` module creates a `Threads::Threads` target that works on all
platforms.

### Googletest

Googletest is not installed, nor should it be, and therefore it is only used
as a submodule.

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

## Implementation

`google-cloud-cpp` defines a `GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER`
configuration option to control where to find all the dependencies. The macro
 can take the following values:
 
 
* `external` implies that all the sources should be downloaded using the
`ExternalProject` CMake module.
* `package` implies that all the dependencies are already installed and can be
found with `find_package()`.
* `pkg-config` is used when the dependencies are already installed but cannot be
found with `find_package()`, instead they can be found using the `pkg-config`
tool.

We will create a separate file in `cmake/Include<Dependency>.cmake` for each of
the dependencies. The file will typically be of this form:

```cmake
set(GOOGLE_CLOUD_CPP_GRPC_PROVIDER "module" CACHE STRING "How to find the gRPC library")
set_property(CACHE GOOGLE_CLOUD_CPP_GRPC_PROVIDER PROPERTY STRINGS "module" "package" "pkg-config")
if ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "external")
    include(external/grpc)
    # Define aliases if needed.
    add_library(gRPC::grpc++ INTERFACE IMPORTED)
    # ... some code ommitted ...
elseif ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "package")
    find_package(GRPCPP REQUIRED >= 1.8)
    find_package(GRPC REQUIRED)
else ()
    # Find the packages using `pkg-config`:
    include(FindPkgConfig)
    pkg_check_modules(gRPC++ REQUIRED IMPORTED_TARGET grpc++ >= 1.9.0)
    add_library(gRPC::grpc++ INTERFACE IMPORTED)
    # This custom macro sets all the properties based on pkg-config results.
    set_library_properties_from_pkg_config(gRPC::grpc++ gRPC++)
endif ()
```

## Testing

The configurations must be tested as part of the CI builds. To ensure coverage
we will:

* Most builds will compile the dependencies from source, as external projects.
  This is how the `google-cloud-cpp` developers use the system and we expect
  that many of the users will too.
  
* One or more of the builds install all the dependencies and then use `package`.

* On Windows, we already use installed dependencies via `vcpkg`: compiling from
  source can be (a) so slow that we go over the time allocated in the CI build,
  and (b) the build may require patching the dependencies, as the `vcpkg` ports
  do.

* We will dedicate one CI build on Linux to compile against installed
  dependencies using `pkg-config` files.

## Alternatives Considered

**Create support files for each dependency and always use `find_package`**: the
author ([@coryan](https://github.com/coryan)) could not design a solution that
works for modules, vcpkg, pkg-config, and system-wide `Find<dependency>.cmake`
files.  It might be possible to do so and if found that would be a more elegant
design than the if statements.

## Estimated Effort

N/A this document was written as the author explored how to solve the problem.
There is a
[branch](https://github.com/coryan/google-cloud-cpp/tree/test-install-target-v2)
that implements all of this.

[cmake-doc-export]:    https://cmake.org/cmake/help/v3.5/command/export.html
[cmake-doc-externalproject]: https://cmake.org/cmake/help/v3.5/module/ExternalProject.html
[cmake-doc-interface]: https://cmake.org/cmake/help/v3.5/command/add_library.html?highlight=interface
[cmake-doc-packages]:  https://cmake.org/cmake/help/v3.5/manual/cmake-packages.7.html#manual:cmake-packages(7)
[cmake-doc-targets]:   https://cmake.org/cmake/help/v3.5/manual/cmake-buildsystem.7.html#binary-targets
[cmake-doc-target-properties]: https://cmake.org/cmake/help/v3.5/manual/cmake-properties.7.html#properties-on-targets
