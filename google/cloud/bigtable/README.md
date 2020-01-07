# Google Cloud Bigtable

Cloud Bigtable is Google's NoSQL Big Data database service. It's the same
database that powers many core Google services, including Search, Analytics,
Maps, and Gmail.

This directory contains the implementation of the Cloud Bigtable C++ client.

[![Documentation][doxygen-shield]][doxygen-link]

[doxygen-shield]: https://img.shields.io/badge/documentation-master-brightgreen.svg
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-bigtable/latest/
[quickstart-link]: https://googleapis.dev/cpp/google-cloud-bigtable/latest/bigtable-quickstart.html
[cbt-docs-link]: https://cloud.google.com/bigtable/docs

## Status

This library support Cloud Bigtable at the
[GA](../../../README.md#versioning) quality level. Please note that, as is
often the case with C++ libraries, we do **not** follow semantic versioning in
the Cloud C++ client libraries. We make every effort to document
backwards-incompatible API changes in the [release notes](#release-notes) below.

## Documentation

* The [client library reference][doxygen-link] contains full documentation
  for the Cloud Storage C++ client library, and includes code samples for all
  APIs.
* A [quick start][quickstart-link] is part of the client library reference.
* Please consult the [Cloud Bigtable website][cbt-docs-link] for general
  documentation on Cloud Bigtable features, APIs, other client libraries, etc.

## Contributing changes

See [`CONTRIBUTING.md`](../../../CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes as well
as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](../../../LICENSE) for details.

## Release Notes

### v1.7.x - TBD

### v1.6.x - 2020-01

* feat: new functions to create Chain and Interleave filters from ranges of Filter objects

### v1.5.x - 2019-12
* feat: implement Bigtable sync vs. async benchmark (#3276)
* fix: detect duplicate cluster ids in `bigtable::InstanceConfig` (#3262)
* bug: use `CalculateDefaultConnectionPoolSize` in `set_connection_pool_size` (#3261)

### v1.4.x - 2019-11

* **Breaking Changes**
    * The common libraries have been moved
      to the [google-cloud-cpp-common repository][github-cpp-common].
      While this may not be a technically breaking change (the API and ABI
      remain unchanged, the include paths are the same), it will require
      application developers to change their build scripts.
      [github-cpp-common]: https://github.com/googleapis/google-cloud-cpp-common
    * Submodule builds no longer supported.
* bug: pass along error message in Table:Apply retry loop (#3208)

### v1.3.x - 2019-10

* bug: fix runtime install directory (#3063)

### v1.2.x - 2019-09

* feat: Configure C++17 build. (#2961)
* fix: use MetadataUpdatePolicy::FromClusterId. (#2968)
* fix: correct invalid routing headers. (#2988)

### v1.1.x - 2019-08

* feat: Minimize contention in Bigtable Client initialization. (#2923)
* feat: Support setting 64-bit integers mutations. (#2866)
* feat: Implement support for IAM conditions. (#2854)
* **BREAKING CHANGE**: use cmake files from
  github.com/googleapis/cpp-cmakefiles for googleapis protos (#2888)

### v1.0.x - 2019-07

* bug: the library will return an error instead of simply discarding unknown IAM
  fields.
* feature: update googleapis protos to a more recent version.
* cleanup: marked rarely used CMake options as advanced. They will no longer
  show up by default in your CMake UI.
* Several internal cleanups, such as removing unused code in
  `google::cloud::bigtable::internal`, fixing constant names to follow the
   Google Style Guide, simplify the generation of version metadata, make it
   easier to import the code into Google, turned on `-Wextra` for our builds,
   moved the sanitizer builds to Bazel, and refactoring generic gRPC utilities
   to a new common library.

### v0.10.x - 2019-06

* **Breaking Changes**
  * The return type for WaitForConsistencyCheck() was a `future<StatusOr<bool>>`
    where most related functions return a `bigtable::Consistency` enum.
  * `Table::CheckAndMutateRow` returns `StatusOr<bool>` to indicate which
    branch of the predicate was taken in the atomic change. Meanwhile,
    `AsyncCheckAndMutateRow()` returned a `future<StatusOr<proto-with-long-name>>`.
    Changed the async and sync versions to return
    `future<StatusOr<MutationBranch>>`. `MutationBranch` is an `enum` as
    `StatusOr<bool>` is too eay to use incorrectly.
  * Removed the `Collection` template parameter from `Table::SampleRows`.
  * Fixed the last function, `WaitForConsistencyCheck`, that returned
    `std::future` to return `google::cloud::future<>`. The function name
    changed too, to be more consistent with similar functions.
  * Remove all the "strong types" for bigtable, such as `InstanceId`,
    `ClusterId`, `TableId`, etc. This changed some of the constructors for
    `bigtable::Table` and several member functions in `bigtable::Table`,
    `bigtable::TableAdmin`, and `bigtable::InstanceAdmin`.
* Changes:
  * Implemented TableAdmin::AsyncWaitForConsistency.
  * Implemented Table::AsyncReadRows.
  * DeleteAppProfile defaults to `ignore_warnings=true`.

### v0.9.x - 2019-05

* **Breaking Changes**
  * Return `google::cloud::future` from `InstanceAdmin` functions: this is
    more consistent with all other functions returning futures.
  * Remove unused `bigtable::GrpcError`: the library no longer raises this
    exception, any code trying to catch the exception should be modified to
    handle errors via `StatusOr<T>`.
  * Remove Snapshot-related functions, tests, examples, etc.: this is
    whitelisted functionality in Cloud Bigtable and it is no longer expected
    to reach GA.
* Continue to implement more async APIs (`Async*()` methods) for the
  `InstanceAdmin`, `TableAdmin`, and `Table` classes
  (**Note: These are not yet stable**)
* Bugfixes:
  * Need `ignore_warnings` to actually delete an AppProfile.
  * Fix portability/logical errors in shell scripts.
  * Fix a race condition in `MutationBatcher`.
* Implemented a number of previously missing code samples.

### v0.8.x - 2019-04

* Use SFINAE to constrain applicability of the BulkMutation(M&&...) ctor.
* **Breaking change**: `Table::BulkApply` now returns a
  `std::vector<FailedMutation>` instead of throwing an exception.
* In the future we will remove all the `google::cloud::bigtable::noex::*`
  classes. We are moving the implementation to `google::cloud::bigtable::*`.
* Continuing to implement more async APIs (Note: These are not yet stable):
  * InstanceAdmin::AsyncDeleteInstance
  * Table::AsyncCheckAndMutateRow
  * TableAdmin::AsyncDeleteTable
  * TableAdmin::AsyncModifyColumnFamilies
* BulkMutator now returns more specific errors instead of generic UNKNOWN.
* Improved install instructions, which are now tested with our CI builds.
* CMake-config files now work without `pkg-config`.
* Removed the googleapis submodule. The build system now automatically
  downloads all deps.
* No longer throw exceptions from `ClientOptions`.

### v0.7.x - 2019-03

* **Breaking change**: Return `StatusOr<>` from `TableAdmin` and `InstanceAdmin`
  operations to signal errors.
* Add streaming to `(Async)BulkMutator`.
* Implement a helper class (`MutationBatcher`) to automatically batch and manage
  outstanding bulk mutations.
* Add `bigtable::Cell` constructors without labels argument.
* Implementation of `RowSet` example using discontinuous keys.
* `List{Instances,Clusters}` return `failed_locations`.
* First version of async `Apply` batching.
* Keep `Apply` callbacks in `MutationData`.

### v0.6.x - 2019-02

* Moved repo organization from GoogleCloudPlatform -> googleapis.
* Implemented several more async functions.
* **Breaking change**: Started migrating functions to `StatusOr` and away from
  throwing exceptions.
* Several fixes to bulk mutator (#1880)
* Disabled `make install` for external projects.
* `Row` now has a move constructor.
* Increased default message length limit.
* Now testing build with libc++ on Linux.
* Fixed some bugs found by Coverity scans.

### v0.5.x - 2019-01

* Restore support for gcc-4.8.
* @remyabel cleaned up some hard-coded zone names in the examples.
* More experimental asynchronous APIs, including AsyncReadRows. Note that we
  expect to change all these experimental APIs as described in
  [#1543](https://github.com/googleapis/google-cloud-cpp/issues/1543).
* @remyabel contributed changes to disable the unit and integration tests. This
  can be useful for package maintainers.
* New Bigtable filter wrapper that accepts a single column.
* **Breaking Change**: remove the `as_proto_move()` memnber functions in favor
  of `as_proto() &&`. With the latter newer compilers will warn if the object
  is used after the destructive operation.

### v0.4.x - 2018-12

* More experimental asynchronous APIs, note that we expect to change all these
  experimental APIs as described in
  [#1543](https://github.com/googleapis/google-cloud-cpp/issues/1543).
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
