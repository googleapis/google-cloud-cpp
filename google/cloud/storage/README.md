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

### v1.7.x - TBD

### v1.6.x - 2019-11

* **Breaking Changes**
    * The common libraries have been moved
      to the [google-cloud-cpp-common repository][github-cpp-common].
      While this may not be a technically breaking change (the API and ABI
      remain unchanged, the include paths are the same), it will require
      application developers to change their build scripts.
      [github-cpp-common]: https://github.com/googleapis/google-cloud-cpp-common
    * Submodule builds no longer supported.
* feat: implement `ComposeMany` to efficiently compose more than 32 GCS objects
* feat: implement a function to delete all the objects that match a given prefix
* feat: support uniform bucket level access #3186
* bug: use separate policy instances for each UploadChunk request #3213

### v1.5.x - 2019-10

* feat: treat `CURLE_SSL_CONNECT_ERROR as retryable` (#3077)
* feat: Change JSON endpoint for `google/cloud/storage`. (#3076)
* bug: restart downloads with retryable HTTP errors. (#3072)
* bug: calls to `resumable_session_id()` result in segfaults (#3062)
* feat: added `ReadLast` option for reading object from end (#3058)
* bug: handle `CURLE_PARTIAL_FILE` as recoverable (#3061)
* bug: fix runtime install directory (#3063)
* bug: verify checksums and hashes on `xgetsn()` (#3057)
* bug: return all bytes when stream is closed (#3054)
* feat: reset retry policy for each retry loop (#3050)

### v1.4.x - 2019-09

* feat: Random CRC and MD5 in storage throughput benchmark (#2943)
* feat: Make GCS throughput benchmark record progress. (#2944)
* feat: Increase download and upload buffers. (#2945)
* feat: Increase the threshold for using resumable uploads (#2946)
* cleanup: Don't include nljson.h from public oauth2 headers. (#2954)
* bug: Handle CURLE_{RECV,SEND}_ERROR as StatusCode::kUnavailable. (#2965)
* cleanup: Use ObjectMetadata in ResumableUploadResponse. (#2969)
* bug: do not disable hashes when Disable{MD5,Crc32c} are set to false (#2979)
* feat: Improve ObjectWriteStreambuf by replacing O(n^2) code. (#2989)
* cleanup: use existing function to generate data. (#2992)
* feat: allow applications to timeout stalled downloads. (#2993)
* fix: Actually enable the error injection test. (#2995)
* cleanup: Factor out payload creation from ServiceAccountCredentials:: (#2997)
* cleanup: Add additional testing for credential helpers. (#3004)
* fix: return proper error code from upload metadata (#3005)
* bug: Fix the initial backoff interval. (#3007)
* fix: eliminate a race condition from retry loop (#3013)
* bug: Unit tests are too slow. (#3021)
* fix: don't throw on expired retry policies (#3023)
* bug: ReadObject() retries only once (#3028)
* bug: CurlRequestBuilder not initializing all members. (#3035)
* fix: use next_expected_byte() in retried uploads (#3037)

### v1.3.x - 2019-08

* feat: Control TCP memory usage in GCS library. (#2902)
* feat: Make partial errors/last_status available to `ObjectWriteStream` (#2919)
* feat: Change storage/benchmarks to compile with exceptions disabled. (#2916)
* feat: Implement native IAM operations for GCS. (#2900)
* feat: Helpers for PredefinedDefaultObjectAcl. (#2885)
* bug: Fix ReadObject() when reading the last chunk. (#2864)
* bug: Use correct field name for MD5 hash. (#2876)

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
