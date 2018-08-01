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
