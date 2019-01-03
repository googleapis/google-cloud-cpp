# Google Cloud Bigtable

[![Documentation][doxygen-shield]][doxygen-link]

[doxygen-shield]: https://img.shields.io/badge/documentation-master-brightgreen.svg
[doxygen-link]: http://GoogleCloudPlatform.github.io/google-cloud-cpp/
[quickstart-link]: http://GoogleCloudPlatform.github.io/google-cloud-cpp/

This directory contains the implementation of the Google Cloud Bigtable C++
client.

## Status

This library support Google Cloud Bigtable at the
[Beta](../README.md#versioning) quality level.

## Documentation

Full documentation, including a [quick start guide][quickstart-link] 
is available [online][doxygen-link].

## Release Notes

### v0.5.x - 2019-01

* Restore support for gcc-4.8.
* @remyabel cleaned up some hard-coded zone names in the examples.
* More experimental asynchronous APIs, including AsyncReadRows. Note that we
  expect to change all these experimental APIs as described in
  [#1543](https://github.com/GoogleCloudPlatform/google-cloud-cpp/issues/1543).
* @remyabel contributed changes to disable the unit and integration tests. This
  can be useful for package maintainers.
* New Bigtable filter wrapper that accepts a single column.
* **Breaking Change**: remove the `as_proto_move()` memnber functions in favor
  of `as_proto() &&`. With the latter newer compilers will warn if the object
  is used after the destructive operation.

### v0.4.x - 2018-12

* More experimental asynchronous APIs, note that we expect to change all these
  experimental APIs as described in
  [#1543](https://github.com/GoogleCloudPlatform/google-cloud-cpp/issues/1543).
* Most of the admin operations now have asynchronous APIs.
* All asynchronous APIs in `noex::*` return an object through which applications
  can request cancellation of pending requests.
* Prototype asynchronous APIs returning a `google::cloud::future<T>`,
  applications can attach callbacks and/or block on a
  `google::cloud::future<T>`.

### v0.3.0 - 2018-11

* Include an example that illustrates how to use OpenCensus and the Cloud
  Bigtable C++ client library.
* Experimental asynchronous APIs.
* Several cleanups around dependency management with CMake.
* Jason Zaman contributed improvements and fixes to support soversion numbers
  with CMake.
* Lots of improvements to the code coverage in the examples and tests.
* Fixed multiple documentation issues, including a much better landing page
  in the Doxygen documentation.

### v0.2.0 - 2018-08

* First Beta release.
* Completed instance admin APIs.
* Completed documentation and examples.
* The API is considered stable, we do not expect any changes before 1.0

### v0.1.0 - 2018-03

* This release implements all the APIs for table and data manipulation in Cloud
  Bigtable.
* This release does not implement APIs to manipulate instances, please use the
  Google Cloud Bigtable
  [command line tool](https://cloud.google.com/bigtable/docs/go/cbt-overview)
  or any of the other programming language APIs instead.
* Only synchronous (blocking) APIs are implemented, asynchronous APIs are in
  our roadmap.
