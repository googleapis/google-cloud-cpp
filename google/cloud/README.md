# Google Cloud C++ Client Libraries

[![Documentation][doxygen-shield]][doxygen-link]

[doxygen-shield]: https://img.shields.io/badge/documentation-master-brightgreen.svg
[doxygen-link]: http://GoogleCloudPlatform.github.io/google-cloud-cpp/
[quickstart-link]: http://GoogleCloudPlatform.github.io/google-cloud-cpp/

This directory contains the following C++ client libraries:
 
* [Cloud Bigtable](bigtable/README.md)
* [Google Cloud Storage](storage/README.md)

In addition some utilities shared across these libraries are also found here.
Most of these utilities are in the `google::cloud::internal` namespace, and are
not intended for direct use by consumers of the C++ client libraries.

## Documentation

Documentation for the common utilities is  
is available [online][doxygen-link].

## Release Notes

### v0.4.x - TBD

### v0.3.x - 2019-01

* Support compiling with gcc-4.8.
* Fix `GCP_LOG()` macro so it works on platforms that define a `DEBUG`
  pre-processor symbol.
* Use different PRNG sequences for each backoff instance, previously all the
  clones of a backoff policy shared the same sequence.
* Workaround build problems with Xcode 7.3.

### v0.2.x - 2018-12

* Implement `google::cloud::future<T>` and `google::cloud::promise<T>` based on
  ISO/IEC TS 19571:2016, the "C++ Extensions for Concurrency" technical
  specification, also known as "futures with continuations". 

### v0.1.x - 2018-11

* `google::cloud::optional<T>` an intentionally incomplete implementation of
  `std::optional<T>` to support C++11 and C++14 users.
* Applications can configure `google::cloud::LogSink` to enable logging in some
  of the libraries and to redirect the logs to their preferred destination.
  The libraries do not enable any logging by default, not even to `stderr`.
* `google::cloud::SetTerminateHandler()` allows applications compiled without
  exceptions, but using the APIs that rely on exceptions to report errors, to
  configure how the application terminates when an unrecoverable error is
  detected by the libraries.
