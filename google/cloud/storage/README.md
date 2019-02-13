# Google Cloud Storage

[![Documentation][doxygen-shield]][doxygen-link]

[doxygen-shield]: https://img.shields.io/badge/documentation-master-brightgreen.svg
[doxygen-link]: http://googleapis.github.io/google-cloud-cpp/latest/storage

This directory contains the implementation of the Google Cloud Storage C++
client.

## Status

This library support Google Cloud Storage at the
[Beta](../README.md#versioning) quality level.

## Documentation

The [reference guide][doxygen-link], can be found online. The top-level page in
the reference guide includes a quick start guide.

## Release Notes

### v0.5.x - TBD

### v0.4.x - 2019-02

* **Breaking change**: Removed almost all exception throwing in favor of
  `StatusOr<T>` return values.
* Lots of cleanup to documentation and example code.
* Avoids use of `StatusOr::value()` when the validity was already checked.
* `Client::ListBuckets()` now directly returns `ListBucketsReader`, because it
  cannot fail so `StatusOr` was not needed.
* Removed support for `StatusOr<void>`; changed usages to return `Status`
  instead.
* 502s are now retryable errors from GCS.
* **Breaking change**: `LockBucketRetentionPolicy` returns a `BucketMetadata`
  now instead of `void`.
* Cleaned up documentation and example code.
* Disabled `make install` for external projects.
* Moved repo organization from GoogleCloudPlatform -> googleapis.
* Moved some internal-only APIs out of public interfaces.
* Fixed resuming uploads when the server responds with a 308.

### v0.3.x - 2019-01

* Try to use the exception mask in the IOStream classes
  (`storage::ObjectReadStream` and `storage::ObjectWriteStream`). This allows
  applications to check errors locally via `rdstate()`. Note that applications
  that disable exceptions altogether must check the `status()` member function
  for these IOStream classes. It is impossible to set the `rdstate()` for all
  failures when exceptions are disabled.
* Support reading only a portion of a Blob.
* Support building with gcc-4.8.
* Many internal changes to better support applications that disable exceptions.
  A future release will include APIs that do not raise exceptions for error
  conditions.
* @remyabel contributed changes to disable the unit and integration tests. This
  can be useful for package maintainers.
* Implement a function to create signed URLs (`Client::CreateV2SignedUrl`).
* Support resumable uploads in any upload operation.

### v0.2.x - 2018-12

* Use resumable uploads for large files in `Client::UploadFile()`.
* Implement support for the `userIp` optional query parameter.
* **BREAKING CHANGE** `Client::RewriteObject()`, `Client::CopyObject()`, and
  `Client::ComposeObject` no longer require the `ObjectMetadata` argument.
  Instead use `WithObjectMetadata()`, which can be omitted if you do not need
  to set any metadata attributes in the new object.
* When using OpenSSL-1.0.2 the client library needs to configure the
  [locking callbacks](https://www.openssl.org/docs/man1.0.2/crypto/threads.html)
  for OpenSSL. However, the application may disable this behavior if the
  application developer is going to use their own locking callbacks.
* When refreshing OAuth2 access tokens the client library uses the same retry
  and backoff policies as used for the request itself.
* Applications can set object metadata attributes via the `WithObjectMetadata`
  optional argument to `Client::InsertObjectMedia()`.
* Applications can configure the library to only retry idempotent operations.
* The client library can use Google Compute Engine credentials to access the
  service.

### v0.1.x - 2018-11

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

[github-link]: https://github.com/googleapis/google-cloud-cpp
