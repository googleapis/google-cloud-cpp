# Google Cloud Platform C++ Client Libraries

C++ Idiomatic Clients for [Google Cloud Platform][cloud-platform] services.

[![Travis CI status][travis-shield]][travis-link]
[![AppVeyor CI status][appveyor-shield]][appveyor-link]
[![Codecov Coverage status][codecov-shield]][codecov-link]
[![Documentation][doxygen-shield]][doxygen-link]

- [Google Cloud Platform Documentation][cloud-platform-docs]

[travis-shield]: https://travis-ci.org/GoogleCloudPlatform/google-cloud-cpp.svg?branch=master
[travis-link]: https://travis-ci.org/GoogleCloudPlatform/google-cloud-cpp/builds
[appveyor-shield]: https://ci.appveyor.com/api/projects/status/d6srbtprnie4ufrx/branch/master?svg=true
[appveyor-link]: https://ci.appveyor.com/project/coryan/google-cloud-cpp/branch/master
[codecov-shield]: https://codecov.io/gh/GoogleCloudPlatform/google-cloud-cpp/branch/master/graph/badge.svg
[codecov-link]: https://codecov.io/gh/GoogleCloudPlatform/google-cloud-cpp
[doxygen-shield]: https://img.shields.io/badge/documentation-master-brightgreen.svg
[doxygen-link]: http://GoogleCloudPlatform.github.io/google-cloud-cpp/
[cloud-platform]: https://cloud.google.com/
[cloud-platform-docs]: https://cloud.google.com/docs/

This library supports the following Google Cloud Platform services with clients
at an [Alpha](#versioning) quality level:

- [Google Cloud Bigtable](bigtable)

The libraries in this code base likely do not (yet) cover all the available
APIs. See the [`googleapis` repo](https://github.com/googleapis/googleapis)
for the full list of APIs callable using gRPC.

## Quick Start

To build the available libraries and run the tests, run the following commands
after cloning this repo:

```bash
git submodule init
git submodule update --init --recursive
cmake -H. -Bbuild-output
cmake --build build-output
(cd build-output && ctest --output-on-failure)
```

On Linux and macOS you can speed up the build by replacing the
`cmake --build build-output` step with:

```bash
cmake --build build-output -- -j $(nproc)
```

On Windows with MSVC use:

```bash
cmake --build build-output -- /m
```

Consult the `README.md` file for each library for links to the examples and
tutorials.

## Build Dependencies

#### Compiler

The Google Cloud C++ libraries are tested with the following compilers:

| Compiler    | Minimum Version |
| ----------- | --------------- |
| GCC         | 4.9 |
| Clang       | 3.8 |
| MSVC++      | 14.1 |
| Apple Clang | 8.1 |

#### Build Tools

The Google Cloud C++ Client Libraries can be built with
[CMake](https://cmake.org) or [Bazel](https://bazel.io).  The minimal versions
of these tools we test with are:

| Tool       | Minimum Version |
| ---------- | --------------- |
| CMake      | 3.5 |
| Bazel      | 0.12.0 |

#### Other Libraries

The libraries also depends on gRPC, libcurl, and the dependencies of those
libraries. The Google Cloud C++ Client libraries are tested with the following
versions of these dependencies:

| Library | Minimum version |
| ------- | --------------- |
| gRPC    | v1.10.x |
| libcurl | 7.47.0  |

For Linux, the `ci/Dockerfile.*` files are a good reference on how to install
all the necessary dependencies on each distribution. For Windows,
consult [ci/install-windows.ps1](ci/install-windows.ps1).

On macOS, the following commands should install all the dependencies you need:

```bash
brew install curl cmake
```

#### Other Dependencies

Some of the integration tests use the
[Google Cloud SDK](https://cloud.google.com/sdk/). The integration tests run
against the latest version of the SDK on each commit and PR.

## Versioning

This library follows [Semantic Versioning](http://semver.org/). Please note it
is currently under active development. Any release versioned 0.x.y is subject to
backwards incompatible changes at any time.

**GA**: Libraries defined at a GA quality level are expected to be stable and
all updates in the libraries are guaranteed to be backwards-compatible. Any
backwards-incompatible changes will lead to the major version increment
(1.x.y -> 2.0.0).

**Beta**: Libraries defined at a Beta quality level are expected to be mostly
stable and we're working towards their release candidate. We will address issues
and requests with a higher priority.

**Alpha**: Libraries defined at an Alpha quality level are still a
work-in-progress and are more likely to get backwards-incompatible updates.
Additionally, it's possible for Alpha libraries to get deprecated and deleted
before ever being promoted to Beta or GA.

## Contributing changes

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details on how to contribute to
this project, including how to build and test your changes as well as how to
properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](LICENSE) for details.
