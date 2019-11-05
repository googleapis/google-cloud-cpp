# Google Cloud Spanner

[![Documentation][doxygen-shield]][doxygen-link]

[doxygen-shield]: https://img.shields.io/badge/documentation-master-brightgreen.svg
[doxygen-link]: http://googleapis.github.io/google-cloud-cpp/
[quickstart-link]: http://googleapis.github.io/google-cloud-cpp/

This directory contains the implementation of the Google Cloud Spanner C++
client.

## Status

This library is under development, we do not recommend that you use it for
production workloads.

## Release Notes

### v0.4.x - TBD

### v0.3.x - 2019-11

* **Breaking Changes**
    * feat!: class templates removed from keys.h (#936)
    * feat!: change result return types (#942)
    * refactor!: replaced Row<...> with std::tuple<...> (#967)
    * feat!: support for select-star queries (#976)
    * feat!: replace RunTransaction() with Client::Commit(functor) (#975)
    * chore!: renamed QueryResult to RowStream (#978)
    * chore!: renamed ExecuteSqlParams to SqlParams (#986)
    * cleanup: depend on google-cloud-cpp-common (#940)

* feat: configurable strategy for background threads (#955)
* feat: add Profile and Analyze methods (#961)
* feat: adds efficient move to support to Value:get<string>() (#980)
* feat: add efficient move support to mutation builder temporaries (#989)
* bug: only install the required headers (#993)
* bug: install the headers for mocking (#997)

### v0.2.x - 2019-10
* **Breaking Changes**
  * refactor `Read` to return `ReadResult`; remove `ResultSet` (#935)
  * removed `Row<>` from mutations API (#938). Removes the `AddRow(Row<Ts...>)`
    member function on the `WriteMutation` API. In place of this method there
    is now an `AddRow(std::vector<Value>)` method.
  * Change `Value::Bytes` to `google::cloud::spanner::Bytes` (#920)
  * implement `CreateInstanceRequestBuilder` (#933). Changed the function
    signature of `InstanceAdminClient::CreateInstance()`.
  * Replace `ExecuteSql` with `ExecuteQuery` and `ExecuteDml` (#927)
  * Changed `RowParser` to require a `Row<Ts...>` template param (#653).
    `ResultSet::Rows` used to be a variadic template that took the individual
     C++ types for each row. With this change that function is now a template
     with one parameter, which must be a `Row<...>` type.
  * Implements `Database` in terms of `Instance` (#652). This PR removes
    renames some accessors like `InstanceId` -> `instance_id` due to their
    trivial nature now (style guide). It also removes some methods like
    `Database::ParentName()`, which is now replaced by `Database::instance()`.
  * Fixes inconsistent naming of the Batch DML params struct. (#650). This
    struct has been renamed, so any code using this struct will need to be
    updated.

* **Feature changes**
  * implement `InstanceAdminClient` methods `CreateInstance`, `UpdateInstance`,
    `DeleteInstance`, `ListInstanceConfigs`, `GetInstanceConfig`,
     `SetIamPolicy`.
  * implement `DatabaseAdminClient` methods `TestIamPermissions`,
    `SetIamPolicy`, `GetIamPolicy`.
  * implement retries for `Commit()`, `PartitionRead`, `PartitionQuery`,
    `ExecuteBatchDml`, `CreateSession`, `Rollback`.
  * implement `PartialResultSetRead` with resumes (#693)
  * use separate policies for retry vs. rerun (#667)
  * implement `DatabaseAdminConnection` (#638)
  * implement overloads on `UpdateInstanceRequestBuilder` (#921)
  * implement metadata decorator for `InstanceAdminStub`. (#678)
  * implement logging wrapper for `InstanceAdminStub`. (#676)
  * support install components for the library (#659)

* **Bug fixes**
  * fix runtime install directory (#658)
  * give sample programs a long timeout time. (#622)
  * use RunTransaction for read write transaction sample (#654)

### v0.1.x - 2019-09
* This is the initial Alpha release of the library.
* While this version is not recommended for production workloads it is stable
  enough to use in experimental code. We welcome feedback about the library
  through [GitHub issues](https://github.com/googleapis/google-cloud-cpp-spanner/issues/new).
* The API is expected to undergo incompatible changes before Beta and GA.
* The library supports all the operations to read and write data into Cloud
  Spanner.
* The library supports some administrative operations such as creating,
  updating, and dropping databases.
