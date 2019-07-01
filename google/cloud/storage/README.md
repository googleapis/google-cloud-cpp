# Google Cloud Storage

Cloud Storage allows world-wide storage and retrieval of any amount of data at
any time. You can use Cloud Storage for a range of scenarios including serving
website content, storing data for archival and disaster recovery, or
distributing large data objects to users via direct download.

This directory contains the implementation of the Google Cloud Storage C++
client.

[![Documentation][doxygen-shield]][doxygen-link]

[doxygen-shield]: https://img.shields.io/badge/documentation-master-brightgreen.svg
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-storage/latest/
[quickstart-link]: https://googleapis.dev/cpp/google-cloud-storage/latest/index.html
[gcs-docs-link]: https://cloud.google.com/storage/docs

## Status

This library support Google Cloud Storage at the
[GA](../../../README.md#versioning) quality level. Please note that, as is
often the case with C++ libraries, we do **not** follow semantic versioning in
the Cloud C++ client libraries. We make every effort to document
backwards-incompatible API changes in the [release notes](#release-notes) below.

## Documentation

* The [client library reference][doxygen-link] contains full documentation
  for the Cloud Storage C++ client library, and includes code samples for all
  APIs.
* A [quick start][quickstart-link] is part of the client library reference.
* Please consult the [Cloud Storage website][gcs-docs-link] for general
  documentation on Cloud Storage features, APIs, other client libraries, etc.

## Contributing changes

See [`CONTRIBUTING.md`](../../../CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes as well
as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](../../../LICENSE) for details.

## Release Notes

### v1.2.x - 2019-07

* **Breaking Change**: we accidentally left two functions in the public API,
  they are now removed. These functions were used to convert
  `google::cloud::storage::ServiceAccount` classes to and from JSON objects.
* **Breaking Change**: we removed the functions in
  `google::cloud::storage::IdempotencyPolicy` for
  `internal::InsertObjectStreamingRequest`. This class is no longer used and the
  functions are unnecessary. This breakage only affects applications that define
  a custom `IdempotencyPolicy`.
* bug: Fixed `WriteObject()` to actually retry the upload for each chunk, not
  just retry the creation of the upload session.
* feature: add examples showing how to mock a `google::cloud::storage::Client`.
* feature: allow applications to load service account credentials from the
  standard locations, but also change the scopes and subject as the credentials
  are loaded. Thanks to @timford for contributing this fix.
* bug: resuming an already finalized upload was not working correctly. Now the
  library restores the stream, but the stream is immediately closed (it is
  incorrect to write more data), and has the object metadata immediately
  available. Thanks to @Jseph for contributing this fix.
* bug: on Windows, `storage::Client::UploadFile()` and
  `storage::Client::DownloadFile()` were always treating the files as text,
  which meant their contents were transformed in unexpected ways. They are now
  always treated as binary.
* bug: we were still leaking a few macros from the nlohmann json library to the
  user's namespace. This is now fixed. Thanks to @remyabel for helping us with
  this.
* feature: reduce data copies during download.
* bug: return an error if the IAM bindings contain unknown fields, previously
  the library was discarding these fields.
* Several internal cleanups, such as fixing constant names to match the Google
  Style Guide, simplify the generation of version metadata, make the integration
  tests more reliable by using several service accounts for each run, use
  `-Wextra` in our builds, and a few more.

### v1.1.x - 2019-06

* Implemented option to read an object starting from an offset.
* Automatically restart downloads on error. With this change the download is
  restarted from the last received byte, and using the object generation used
  in the original download as well. (#2693)
* Bugfixes:
  * Service account credentials not refreshing properly. (#2691)
  * Fix build for macOS+CMake. (#2698)
  * WriteObject now supports empty streams. (#2714)

### v1.0.x - 2019-05

* Declared GA and updated major number.
* Support signed policy documents.
* Support service account key files in PKCS#12 format (aka `.p12`).
* Support signing URLs and policy documents using the SignBlob API, this is
  useful when using the default service account in GCE to sign URLs and policy
  documents.

### v0.6.x - 2019-04

* Added initial support for HMAC key-related functions.
* Added support for V4 signed URLs.
* Improved install instructions, which are now tested with our CI builds.
* CMake-config files now work without `pkg-config`.
* No longer throw exceptions from `ClientOptions`.
* Handle object names with slashes.
* Added `ObjectMetadata::set_storage_class`
* Added support for policy documents.

### v0.5.x - 2019-03

* Properly handle subresources in V2 signed URLs.
* Allow specifying non-default `ServiceAccountCredentials` scope and subject.
* Add `make install` instructions.
* Change the storage examples to throw a `std::runtime_error` on failure.
* Add Bucket Policy Only samples.

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
