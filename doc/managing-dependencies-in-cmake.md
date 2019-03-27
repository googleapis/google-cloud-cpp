# CMake Dependency Management for google-cloud-cpp

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
* [crc32c](https://github.com/google/crc32c) is used by the storage library to
  generate CRC32C checksums.

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

We support two different modes to compile the library with CMake:

- Using [CMake external projects][cmake-doc-externalproject]
- Using pre-installed dependencies.

Bazel builds are basically external project builds.

When compiling with external projects we create [targets][cmake-doc-targets]
that have the same names (and roles) as the `Find<Package>` module creates when
the dependency is installed. For example, when protobuf is installed and 
discovered via `find_package()` the module for protobuf introduces a
`protobuf::libprotobuf` target. We will construct our external projects to
exhibit the same behavior.

CMake supports this use case well as long as projects follow the following
conventions:

* One should use dependencies via their [exported][cmake-doc-export] names, for
  example, `absl::base` and not `absl_base`.
* Dependencies should use `target_include_directories()` and
  `target_compile_definitions()` to add compilation flags.
* Dependencies should create a [CMake-config][cmake-doc-packages] config file as
  part of their installation.

## Detailed Management for all Dependencies

Some of the dependencies require special treatment, these are noted below:

### gRPC

gRPC can be compiled using CMake, plain GNU Make, Bazel, or vcpkg. It may also
be available as a binary package on some distributions. Depending on how it is
compiled it may create CMake-config files to support `find_package()`, but
in many distributions these are not installed. We support two modes to compile
gRPC:

1. `external`: When `GOOGLE_CLOUD_CPP_GRPC_PROVIDER` is set to `external`
   (the default) we will compile gRPC as an external project and install it
   in `<BUILD_DIR>/external`.

1. `package`: When `GOOGLE_CLOUD_CPP_GRPC_PROVIDER` is set to `package` we
    will use `find_package(gRPC)` to find the gRPC libraries. We have
    created a `FindgRPC` module that discovers how gRPC was installed, normally
    using the CMake-config files, but can fallback on `pkg-config` if needed.

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

`protobuf` is covered in more detail below.

`zlib` is often installed as a system library and has a relatively stable
interface.  Unless it is used as a module we will use the native library via
`find_package`.

`libpthread` has native support in CMake and we will simply use that: the
`FindThreads` module creates a `Threads::Threads` target that works on all
platforms.

### Protobuf

Protobuf can also be compiled using CMake, plain GNU Make, Bazel, or vcpkg. It
may also be available as a binary package on some distributions. Depending on
how it is compiled it may have installed CMake-config files. All CMake versions
we support (>=3.5) provide a `FindProtobuf` module, but this module does not
create the correct imported targets until CMake-3.9, and the protobuf compiler
does not get an imported target until CMake-3.10.

1. `external`: When `GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER` is set to `external`
   (the default) we will compile gRPC as an external project and install it
   in `<BUILD_DIR>/external`.

1. `package`: When `GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER` is set to `package` we
    will use `find_package(ProtobufTargets)` to find the Protobuf libraries.
    `FindProtobufTargets` is a module that we wrote, it uses the CMake standard
    `FindProtobuf` module to find the protobuf libraries, but always produces
    the `protobuf::*` targets generated by CMake>=3.11.

### Googletest

Googletest is not installed, nor should it be, and therefore it is only used
as an external project.

### Googleapis Protos

The proto files and the generated libraries are always compiled as an external
project. The build files are in `google-cloud-cpp/cmake/googleapis` and they
generate both CMake-config files and pkg-config files to support CMake and Make
users.

To build with Bazel we have implemented a custom BUILD file in
`bazel/googleapis.BUILD`.

## Implementation

`google-cloud-cpp` defines a `GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER`
configuration option to control where to find all the dependencies. The macro
 can take the following values:

* `external` implies that all the sources should be downloaded using the
  `ExternalProject` CMake module.
* `package` implies that all the dependencies are already installed and can be
  found with `find_package()`.

We keep a separate file in `cmake/Include<Dependency>.cmake` for each of
the dependencies. The file will typically be of this form:

```cmake
set(GOOGLE_CLOUD_CPP_GRPC_PROVIDER "external" CACHE STRING "How to find the gRPC library")
set_property(CACHE GOOGLE_CLOUD_CPP_GRPC_PROVIDER PROPERTY STRINGS "module" "package")
if ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "external")
    include(external/grpc)
elseif ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "package")
    find_package(gRPC REQUIRED >= 1.9.1)
endif ()
```

## Testing

The configurations must be tested as part of the CI builds. To ensure coverage
we:

* Most builds compile the dependencies as external projects. This is how the
  `google-cloud-cpp` developers use the system and we expect that many of the
   users will too.

* On each supported Linux distribution we have a build that:
  * Install all the dependencies.
  * Compile the `google-cloud-cpp` libraries against those installed dependencies.
  * Install `google-cloud-cpp` against those dependencies.
  * Use both `make(1)` and `cmake(1)` to compile against the installed version
    of `google-cloud-cpp`.

* On Windows, we always compile using "installed" dependencies via `vcpkg`.
  Compiling from source can be (a) so slow that we go over the time allocated in
  the CI build, and (b) the build may require patching the dependencies, as the
  `vcpkg` ports do.

* On MacOS, we are only building with Bazel at this time.

* We also have two Bazel builds for Linux:
  * A build to verify `google-cloud-cpp` can be used as a dependency of another
    project.
  * A build to compile the unit and integration tests of `google-cloud-cpp`,
    though only the unit tests are executed.

[cmake-doc-export]:    https://cmake.org/cmake/help/v3.5/command/export.html
[cmake-doc-externalproject]: https://cmake.org/cmake/help/v3.5/module/ExternalProject.html
[cmake-doc-interface]: https://cmake.org/cmake/help/v3.5/command/add_library.html?highlight=interface
[cmake-doc-packages]:  https://cmake.org/cmake/help/v3.5/manual/cmake-packages.7.html#manual:cmake-packages(7)
[cmake-doc-targets]:   https://cmake.org/cmake/help/v3.5/manual/cmake-buildsystem.7.html#binary-targets
[cmake-doc-target-properties]: https://cmake.org/cmake/help/v3.5/manual/cmake-properties.7.html#properties-on-targets
