# Google Cloud Strorage

[![Documentation][doxygen-shield]][doxygen-link]

[doxygen-shield]: https://img.shields.io/badge/documentation-master-brightgreen.svg
[doxygen-link]: http://GoogleCloudPlatform.github.io/google-cloud-cpp/latest/storage

This directory contains the implementation of the Google Cloud Storage C++
client.

## Status

This library support Google Cloud Storage at the
[Alpha](../README.md#versioning) quality level.

## Documentation

The [reference guide][doxygen-link], can be found online. The top-level page in
the reference guide includes a quick start guide.

## Release Notes

### 2018-11

* Automatically compute MD5 hashes and CRC32C checksums when objects are
  uploaded and downloaded. Any hash or checksum mismatched results in an
  exception. Applications can MD5 hashes, CRC32C checksums or both on any
  request.
* Parse Bucket lock and retention policy attributes in object and bucket
  metadata.
* Add APIs to upload and download files with a single function call.
* Improved the error messages generated when the credentials file is missing
  or has invalid contents.
* Jason Zaman contributed improvements and fixes to support soversion numbers
  with CMake.

### 2018-09

* The library implements all the APIs in the service, including:
  * Create, list, and delete buckets and objects.
  * Read and modify metadata attributes of buckets and objects, including patch
    semantics.
  * Compose multiple objects into a single object.
  * Rewrite objects to a different bucket or with with different encrytion keys.
  * Read object contents as a stream of data.
  * Create objects using a stream of data.
  * Create objects from a `std::string`.
  * Control Object lifecycle policies in buckets.
  * Control versioning in buckets.
  * Read, modify and test IAM policies in buckets.
  * Read, modify, list, and set ACLs for buckets and objects.
  * Control Pub/Sub configuration of a bucket.
  * Use pre-conditions for object and bucket operations.
  * Customer-Supplied Encryption keys.
  * Customer-Managed Encryption keys.
* The library does not (yet) have a GitHub release, it is only available from
  the [default branch][github-link] on GitHub

[github-link]: https://github.com/GoogleCloudPlatform/google-cloud-cpp
