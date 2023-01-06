# Common Types and Functions for the Google Cloud C++ Libraries

The source files in `google/cloud/` define types and functions shared by the
several Google Cloud C++ libraries. Here is where you will find the
implementation of types used throughout the libraries, such as
`StatusOr<T>`, `Status`, `future<T>`, or `CompletionQueue`.

We consider this code stable and generally available. Please note that the
Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](http://semver.org/).

Some sub-directories include **internal-only** types and functions. These are
used in the implementation of the Google Cloud C++ libraries, but are **not**
intended for general use. They are subject to change and/or removal without
notice. These include `google/cloud/internal/`, and
`google-cloud/testing_utils/`.

## Supported Platforms

- Windows, macOS, Linux
- C++14 (and higher) compilers (we test with GCC >= 7.3, Clang >= 6.0, and
  MSVC >= 2017)
- Environments with or without exceptions
- Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

- [Reference doxygen documentation][doxygen-link] for each release of this
  library
- Detailed header comments in our [public `.h`][source-link] files

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.

[doxygen-link]: https://googleapis.dev/cpp/google-cloud-common/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud
