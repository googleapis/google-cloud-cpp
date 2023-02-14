# Compile time configuration for google-cloud-cpp

The `google-cloud-cpp` libraries have a number of compile-time configuration
options. This document describes these options and provides some motivation
to use them (or not).

The bulk of this document is about CMake-based builds. Bazel-based builds also
have a small number of options and are also described here.

## CMake Options

We expect that most application developers using CMake will compile and install
`google-cloud-cpp` and then use it as an external dependency. Some application
developers may use `google-cloud-cpp` via `FetchContent()` or some other kind
of super build. In the latter case you should note how this project configures
testing and features.

As usual, application developers can use [ccmake], or run `cmake -L` to discover
all options in the project.

### Enabling and disabling features

The `GOOGLE_CLOUD_CPP_ENABLE` CMake option configures what features are enabled
at compile time.

Most features correspond to a single library. For example,
enabling the `kms` compiles and installs the `google-cloud-cpp::kms` library.

Some features represent groups of libraries. For example, `__ga_libraries__`
requests all the GA libraries, and `__experimental_libraries__` represent the
libraries that are not yet GA.

A few features enable experimental functionality. We do not expect that
customers will need to use these. If you have specific questions please start
a [GitHub Discussion]. With that said:

- `internal-docfx` enables an internal-only tool to generate documentation.
- `generator` enables an internal-only tool to generate new libraries.
- `experimental-storage-grpc` enables the GCS+gRPC plugin. Contact your account
  team if you want to use this feature or are interested in the GA timeline.
- `experimental-http-transcoding` enables support for HTTP/1.1 transport (as
  opposed to gRPC over HTTP/2) in some libraries.
- `experimental-opentelemetry` enables support for [OpenTelemetry].

### Disabling C++ Exceptions

`google-cloud-cpp` does not throw exceptions to signal errors. Though in some
cases the application may call a function to throw an exception on errors.
Some applications require their libraries to be compiled without exception
support. Use `-DGOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS=OFF` if that is the
case.

### Checking OpenSSL version on macOS

A common mistake on macOS is to use the system's OpenSSL with
`google-cloud-cpp`. This configuration is not supported, and the build stops to
provide a better error message. If this error message is not applicable to your
environment use `-DGOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK=OFF`.

### Enabling tests and examples

The tests are enabled via `-DBUILD_TESTING=ON`. You can disable the examples
using  `-DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF`. This may speed up your build
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

The `--host_cxxopt` may be unfamiliar. This is required to support Protobuf
and gRPC, which compile code generators for the "host" environment, and
generate libraries for the "target" environment.

### Disabling OpenTelemetry

[OpenTelemetry] is enabled by default.  Turning this off may reduce your build
times but will also lose the benefits of instrumenting the libraries for
distributed tracing.  Add `--//:experimental-opentelementry=false` to your
Bazel command-line parameters to disable Open Telemetry.

[ccmake]: https://cmake.org/cmake/help/latest/manual/ccmake.1.html
[github discussion]: https://github.com/googleapis/google-cloud-cpp/discussions
[opentelemetry]: https://opentelemetry.io/
