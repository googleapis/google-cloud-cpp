# Google Cloud Platform C++ Client Libraries

[![Travis CI status][travis-shield]][travis-link]

[travis-shield]: https://travis-ci.org/GoogleCloudPlatform/google-cloud-cpp.svg
[travis-link]: https://travis-ci.org/GoogleCloudPlatform/google-cloud-cpp/builds

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
mkdir build
cd build
cmake ..
make all
make test
```

## Contributing changes

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details on how to contribute to
this project.

## Licensing

Apache 2.0; see [`LICENSE`](LICENSE) for details.
