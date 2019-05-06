# Google Cloud Bigtable

Cloud Bigtable is Google's NoSQL Big Data database service. It's the same
database that powers many core Google services, including Search, Analytics,
Maps, and Gmail.

This directory contains the implementation of the Cloud Bigtable C++ client.

[![Documentation][doxygen-shield]][doxygen-link]

[doxygen-shield]: https://img.shields.io/badge/documentation-master-brightgreen.svg
[doxygen-link]: http://googleapis.github.io/google-cloud-cpp/latest/bigtable
[quickstart-link]: https://googleapis.github.io/google-cloud-cpp/latest/bigtable/bigtable-quickstart.html
[cbt-docs-link]: https://cloud.google.com/bigtable/docs

## Status

This library support Cloud Bigtable at the
[Beta](../../../README.md#versioning) quality level. Please note that, as is
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

### v0.9.x - 2019-05
* Continue to implement more async APIs (*Note: These are not yet stable*):
  * `InstanceAdmin`: implement `AsyncSetIamPolicy`, `AsyncDeleteAppProfile`,
    `AsyncGetAppProfile`, `AsyncCreateAppProfile`, `AsyncGetIamPolicy`,
    `AsyncTestIamPermissions`, `AsyncUpdateAppProfile`, `AsyncUpdateInstance`,
    `AsyncCreateCluster`, `AsyncUpdateCluster`, `AsyncCreateInstance`,
    `AsyncListAppProfiles`.
  * `TableAdmin`: Implement `AsyncGenerateConsistencyToken`,
    `AsyncCheckConsistency`, `AsyncListTables`, `AsyncDropAllRows`,
    `AsyncDropRowsByPrefix`.
  * `Table`: Implement `AsyncReadModifyWriteRow`.
  * Return `google::cloud::future` from `InstanceAdmin` functions.
  * Reimplement `InstanceAdmin` methods `AsyncListClusters`,
    `AsyncListInstances` and `AsyncDeleteCluster` via futures.
* Implement examples/samples:
  *  `bigtable_create_replicated_cluster`, `bigtable_check_table_exists`,
     `bigtable_get_or_create_table`, `bigtable_get_or_create_family`,
     `bigtable_list_column_families`, `bigtable_get_family_metadata`,
     `bigtable_row_exists`, `bigtable_read_*`.
  * `CheckAndMutate()`, `Apply()` and `BulkApply()`.
  * Implement remaining column family examples and GC rule examples.
* Cleanups:
  * Remove `noex::Table*` dependencies in `Table::BulkApply()`,
    `Table::*ReadModifyWriteRow()`.
  * Remove `noex::Table::ReadModifyWriteRow()` and `SampleRows()`.
  * Remove unused `bigtable::GrpcError`.
  * Remove unused `AsyncFutureFromCallback`.
  * Remove Snapshot-related functions, tests, examples, etc.
* Bugfixes:
  * Need `ignore_warnings` to actually delete an AppProfile.
  * Fix portability/logical errors in shell scripts.
  * Fix a race condition in `MutationBatcher`.
* Test-only changes:
  * Improve error messages in integration test.
  * Fix the admin tests and examples to run against production.
  * Prevent transient problems from breaking production test.
  * Fix race condition in `async_read_stream_test`.
  * Bypass creating real alarms in `MockCompletionQueue`.
* **Dropped changes**
  * from 0.8.1:
    * Avoid `std::make_exception_ptr()` in `future_shared_state_base::abandon()`
    * Use SFINAE to constrain applicability of the `BulkMutation(M&&...)` ctor
  * below threshold for inclusion:
    * Refactor BulkMutator state to a separate class.
    * Rename AsyncBulkApply support classes.
    * Wrap streaming read RPC loop.
    * Explicit return types in `data_async_snippets.cc`
    * Use explicit return type in `data_snippets.cc`
    * Use explicit return types in `bigtable_instance_admin_snippets.cc`
    * Use explicit return types in `instance_admin_async_snippets.cc`
    * Fix broken links and link INSTALL.md directly.
    * Move to clang-format-7
    * Add the "IncludeBlocks: Merge" clang-format option
    * Check for (and fix) whitespace formatting in non-C++ files
    * Coalesce the separated #include blocks
    * Moves enablers to LHS of assignment to prevent callers specifying.
    * Add v0.8.1 release notes
    * Purge some newly-introduced trailing whitespace.
    * Enclose instances of $0 in quotes
    * Fix typos in these READMEs: `support->supports`
    * Clean up relational operators
    * Cleanup the Doxygen documentation.
    * Update documentation for top-level classes.
    * Cleanup the InstanceAdmin snippets.
    * Fix typos and comments in the CompletionQueue test.
    * Create aliases for table schema view constants.
    * Remove namespace aliases from header files.
    * Remove polling loops from samples with future.
    * Consistently use namespace aliases in examples.
    * Fix exit status in examples runner.
    * Adding delete table region tag for hello world
    * Use the `bigtable_get_table_metadata` region tag.
    * Add `bigtable_delete_rows` region tag.
    * Remove invalid region tags.
    * Fix region tags for `bigtable_hello_world`.
    * Fix typos in region tags.

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
