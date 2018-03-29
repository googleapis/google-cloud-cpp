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

```sh
git submodule init
git submodule update --init --recursive
mkdir build-output
cd build-output
cmake ..
make all
make test
```

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
