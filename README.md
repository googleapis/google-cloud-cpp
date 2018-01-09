# Google Cloud Platform C++ Client Libraries

[![Travis CI status][travis-shield]][travis-link]
[![Codecov Coverage status][codecov-shield]][codecov-link]

[travis-shield]: https://travis-ci.org/GoogleCloudPlatform/google-cloud-cpp.svg?branch=master
[travis-link]: https://travis-ci.org/GoogleCloudPlatform/google-cloud-cpp/builds
[codecov-shield]: https://codecov.io/gh/GoogleCloudPlatform/google-cloud-cpp/branch/master/graph/badge.svg
[codecov-link]: https://codecov.io/gh/GoogleCloudPlatform/google-cloud-cpp

This repo contains experimental client libraries for the following APIs:

* [Google Cloud Bigtable](bigtable/)

The libraries in this code base likely do not (yet) cover all the available
APIs. See the [`googleapis` repo](https://github.com/googleapis/googleapis)
for the full list of APIs callable using gRPC.

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

## Contributing changes

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details on how to contribute to
this project, including how to build and test your changes as well as how to
properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](LICENSE) for details.
