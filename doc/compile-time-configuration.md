# Compile time configuration for google-cloud-cpp

The `google-cloud-cpp` libraries have a number of compile-time configuration
options. This document describes these options and provides some motivation to
use them (or not).

The bulk of this document is about CMake-based builds. Bazel-based builds also
have a small number of options and are also described here.

## CMake Options

We expect that most application developers using CMake will compile and install
`google-cloud-cpp` and then use it as an external dependency. Some application
developers may use `google-cloud-cpp` via `FetchContent()` or some other kind of
super build. In the latter case you should note how this project configures
testing and features.

As usual, application developers can use [ccmake], or run `cmake -L` to discover
all options in the project.

### Enabling and disabling features

The `GOOGLE_CLOUD_CPP_ENABLE` CMake option configures what features are enabled
at compile time.

Most features correspond to a single library. For example, enabling the `kms`
compiles and installs the `google-cloud-cpp::kms` library.

Some features represent groups of libraries. For example, `__ga_libraries__`
requests all the GA libraries, and `__experimental_libraries__` represent the
libraries that are not yet GA.

A few features enable experimental functionality. We do not expect that
customers will need to use these. If you have specific questions please start a
[GitHub Discussion]. With that said:

- `generator` enables an internal-only tool to generate new libraries.
- `experimental-http-transcoding` enables support for HTTP/1.1 transport (as
  opposed to gRPC over HTTP/2) in some libraries.
- `opentelemetry` enables support for [OpenTelemetry].

### Override Protobuf compiler and gRPC's plugin

`google-cloud-cpp` uses the protobuf compiler and gRPC's plugin to generate code
from the Protobuf API definitions. By default it finds these tools using the
targets exported by `find_package(Protobuf CONFIG)` and
`find_package(gRPC CONFIG)`. These defaults do not work when cross-compiling, as
the packages will point to the tools for the **target** environment, and they
may not run in the host environment, where the build is running.

You can override these tools using `-DProtobuf_PROTOC_EXECUTABLE=/path/...` and
`-DGOOGLE_CLOUD_CPP_GRPC_PLUGIN_EXECUTABLE=/other-path/...`.

### Disabling C++ Exceptions

`google-cloud-cpp` does not throw exceptions to signal errors. Though in some
cases the application may call a function to throw an exception on errors. Some
applications require their libraries to be compiled without exception support.
Use `-DGOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS=OFF` if that is the case.

### Checking OpenSSL version on macOS

A common mistake on macOS is to use the system's OpenSSL with
`google-cloud-cpp`. This configuration is not supported, and the build stops to
provide a better error message. If this error message is not applicable to your
environment use `-DGOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK=OFF`.

### Enabling tests and examples

The tests are enabled via `-DBUILD_TESTING=ON`. You can disable the examples
using `-DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF`. This may speed up your build
times.

## Bazel Options

We expect that application developers using Bazel will include
`google-cloud-cpp` as an external dependency.

### The C++ Standard

The default Bazel toolchain forces C++11 on Linux and macOS. `google-cloud-cpp`
requires C++14, so you will need to update the C++ standard version. You must
either:

- Provide your own C++ toolchain configuration for Bazel. Consult the Bazel
  documentation if you want to attempt this approach.
- Add `--cxxopt=-std=c++14` and `--host_cxxopt=-std=c++14` (or higher) in your
  Bazel command-line.
- Add the same options in your Bazel `.bazelrc` file.

The `--host_cxxopt` may be unfamiliar. This is required to support Protobuf and
gRPC, which compile code generators for the "host" environment, and generate
libraries for the "target" environment.

### Enabling OpenTelemetry

[OpenTelemetry] is disabled by default. Add `--//:enable_opentelemetry` to your
Bazel command-line parameters to enable OpenTelemetry features, such as
instrumentation to collect distributed traces.

See the [OpenTelemetry quickstart] for more details.

### Workarounds

gRPC circa 1.58.1 fails to build with Bazel and Clang >= 16 (see [grpc#34482]).
Add the `--features=-layering_check` option to your Bazel command-line or add
`build: --features=-layering_check` to your `.bazelrc` file.

[ccmake]: https://cmake.org/cmake/help/latest/manual/ccmake.1.html
[github discussion]: https://github.com/googleapis/google-cloud-cpp/discussions
[grpc#34482]: https://github.com/grpc/grpc/issues/34482
[opentelemetry]: https://opentelemetry.io/
[opentelemetry quickstart]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/opentelemetry/quickstart
