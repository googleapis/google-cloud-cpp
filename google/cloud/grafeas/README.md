# Grafeas Proto Libraries

:construction:

This directory contains **experimental** libraries. Its contents are subject to
change without notice.

This directory contains CMake targets to compile the
[Grafeas](https://grafeas.io) protocol buffer files as a C++ library. Grafeas
(the Greek word for "Scribe") is an open artifact metadata API to audit and
govern your software supply chain.

Several Google Cloud Platform services use the Grafeas APIs and protocol buffer
definitions. This directory contains CMake targets to generate the C++ libraries
corresponding to this code.  Customers are not expected to use these libraries
directly (though they might), instead we recommend using the idiomatic C++
libraries for the GCP services.

Please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Supported Platforms

* Windows, macOS, Linux
* C++11 (and higher) compilers (we test with GCC >= 5.4, Clang >= 6.0, and MSVC >= 2017)
* Environments with or without exceptions
* Bazel (>= 4.0) and CMake (>= 3.5) builds

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.
