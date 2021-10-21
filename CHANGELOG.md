# Changelog

## FUTURE BREAKING CHANGES:

<!-- Keep these sorted by estimated date -->

<details>
<summary>2021-05-01: removing super build test files.</summary>

* Shortly after 2021-05-01 we are planning to remove the super build test files
  in the `super/` directory. `google-cloud-cpp` should remain usable in a
  super build for a larger project, but we think this testing is redundant
  with other testing that installs `google-cloud-cpp` in `$HOME`. Contributors
  to `google-cloud-cpp` can use `vcpkg` to install the dependencies as part of
  their CMake setup.
</details>

<details>
<summary>2022-02-15: retiring legacy targets and rules</summary>
<br>

* In 2022-02-15 (or around that time) we are planning to remove a number of
  backwards compatibility targets names for Bazel and CMake.  Specifically:
  - **Bazel Users:** applications should use the targets at the top-level
    directory, e.g. `//:bigtable`, or `//:pubsub`.
    - All other Bazel targets will be marked as package private in or around
      2022-02-15.
  - **CMake Users:** applications should use the
    `google-cloud-cpp::*` targets (e.g. `google-cloud-cpp::pubsub`).
    - The legacy CMake targets generate warnings if using CMake >= 3.18 (the
      earliest version that supports deprecation warnings).
    - All exported targets without a `google-cloud-cpp::` prefix will be
      retired in or around 2022-02-15. These include, but are not limited to:
      - Any target starting with `googleapis-c++::`
      - Any exported targets without a prefix, including:
        `google_cloud_cpp_common`, `google_cloud_cpp_grpc_utils`,
        `bigtable_client`, `bigtable_protos`, `firestore_client`,
        `pubsub_client`, `storage_client`, `spanner_client`.
      - Some target aliases, including `bigtable::client`, `bigtable::protos`,
        `firestore::client`
  - **pkg-config users:** applications should use the modules starting with
    `google_cloud_cpp`
    - All other modules will be retired in or around 2022-02-15
  - **Direct users of -l${library} flags:** we do not recommend that
    applications uses `-l` flags directly, please use `pkg-config` and/or
    the target names under CMake or Bazel. We make this recommendation because
    we do not know of any mechanism to provide backwards compatibility for such
    flags.
  - If you have any feedback about this change please add comments in
    [#5726](https://github.com/googleapis/google-cloud-cpp/issues/5726)
</details>

<details>
<summary>2022-04-01: retiring legacy IAM functions</summary>
<br>

* On 2022-04-01 (or shortly after) we are planning to remove a number of
  IAM functions designed before [IAM conditions][iam-conditions-link]. These
  functions do not work with IAM policies that include IAM conditions, and
  have been marked as deprecated since before 2019-08-01, albeit in Doxygen
  comments only. Starting with the v1.25.0 release, and depending on your
  compiler settings, using these functions may issue a deprecation warning at
  the call site. See [#5929] for more details.
</details>

## v1.30.0 - TBD

### Bigtable:

**BREAKING CHANGES**:
* `Async*` functions in `Table` that took `CompletionQueue&` as a parameter
  have been removed. Users that need to update their code should use the
  identically named functions which do not require a `CompletionQueue&`. If
  users absolutely require a custom `CompletionQueue`, one can be supplied to
  the `DataClient` used to construct the `Table`, via
  `ClientOptions::DisableBackgroundThreads(CompletionQueue const&)`.
  See [#2567][issue-2567] for more details.

[issue-2567]: https://github.com/googleapis/google-cloud-cpp/issues/2567

## v1.29.0 - 2021-07

### Bigtable:

**BREAKING CHANGES**:

* Experimental functions matching `Async*` in `bigtable::TableAdmin` and
  `bigtable::InstanceAdmin` have been removed from the public API. See
  [#5923](https://github.com/googleapis/google-cloud-cpp/issues/5923) for more
  details.

**Other changes**:

* feat(bigtable): add Backup IAM Policy snippets (#6847)
* doc(bigtable): revamp examples README (#6839)
* docs: fix connection pool size for samples (#6834)
* cleanup(doc): add IAM examples to Bigtable gRPC doc, fix typos (#6838)
* cleanup(bigtable)!: remove admin async methods from public API (#6711)

### Pub/Sub:

* fix: cast to std::string which works with string and Cord (#6850)
* doc(pubsub): an example for publisher flow control (#6784)
* feat(pubsub): flow control for Publisher (#6678)
* feat(pubsub): use unary RPCs for Ack/Nack (#6674)
* feat(pubsub): direct Ack/Nack through SubscriberStub (#6666)

### Storage:

* fix(storage): allow overwrites in parallel uploads (#6908)
* feat(GCS+gRPC): capture streaming RPCs metadata (#6902)
* fix(storage): use correct header for XML pre-conditions (#6903)
* fix(storage): use JSON for reads with pre-conditions (#6900)
* fix(storage): treat 304 errors as kFailedPrecondition (#6897)
* feat(storage): micro-optimization for downloads (#6892)
* fix(GCS+gRPC): avoid duplicate headers in downloads (#6891)
* feat(storage): implement "public access prevention" (#6755)
* fix(GCS+gRPC): resumable uploads can resume (#6881)
* feat(storage): share client in throughput benchmark (#6882)
* feat(storage): make auto-finalization optional (#6874)
* fix(storage): alternative endpoints and Host header (#6864)
* fix(storage): crashes with tracing enabled (#6870)

### Spanner:

* feature(spanner): low-cost instances (#6895)
* refactor(spanner): Use background threads for admin LROs (#6853, #6835)
* feature(spanner): add optimizer_statistics_package to QueryOptions (#6727)
* fix(spanner): pick up tracing-option defaults in SetBasicDefaults() (#6691)

### Common Libraries:

* feat(common): support LROs (long-running operations) use background threads
  (#6855, #6831, #6824, #6823, #6822, #6820, #6816, #6804)
* fix(common): remove blocking call in MinimalIamCredentials (#6813)

## v1.28.0 - 2021-06

### Bigtable

* feat(bigtable): Add asynchronous method for sampling row keys (#6561). Users
  that extend `bigtable::DataClient` and wish to use the new method will need to
  override `PrepareAsyncSampleRowKeys()`.
* fix(bigtable): `bigtable::Table::AsyncBulkApply()` now respects the Retry and
  Backoff policies (#6510)

### IAM
**NOTE** This release includes an **experimental** IAM client library. This
library is **NOT GA** and is likely to have breaking changes in the coming
months. Feel free to take a look, file issues, ask questions, and even
experiment with it, but do not ship production code using it yet.

* feature(iam): generate iam admin code (#6430)

### Pub/Sub

* fix(pubsub): save refresh timer for cancellation (#6662)
* feat(pubsub): add ability to set the ack deadline extension (#6620)

### Spanner

* feat(spanner): use multiple channels instead of multiple client for the
  multiple rows cpu benchmark (#6659)
* fix(spanner): do not warn about SessionPoolClockOption (#6619)

### Storage

* feat(storage): recommend `g::c::Options` (#6640)
* feat(storage): support multiple channels in GCS+gRPC (#6593)
* feat(storage): round-robin stub for GCS+gRPC (#6584)
* fix(storage): compile GCS+gRPC with Windows+x86 (#6556)
* feat(storage): install rules for GCS+gRPC plugin (#6527)
* fix(storage): compile with GCC 11 in C++17 mode (#6501)
* feat: Support unified credentials (`google::cloud::Credentials`) to initialize
  both the REST and gRPC plugins with the same classes. (#6518)(#6531)(#6617)(#6488)(#6441)

### Common Libraries

* feat: Add support for "unified" credentials, which allow applications to
  Configure the REST and gRPC-based libraries using the same credential classes
  Support complex credential types, such as service account impersonation
  With this release, only the `storage` library supports unified credentials.
  (#6617)(#6614)(#6531)(#6518)
* feat: an example using identity tokens with gRPC (#6583)
* feat: example using identity tokens (#6569)

## v1.27.0 - 2021-05

### Storage

* fix(storage): missing implementation for constructor (#6439)
* feat(storage): minimal stub for IAM credentials service (#6425)
* feat: support errors in DynamicAccessTokenCredentials (#6325)
* feat(storage): use the unified authentication client (#6323)
* feat(storage): map unified to REST credentials (#6301)
* refactor(storage): use g::c::Options in RawClients (#6282)
* refactor(storage): common code for client unit tests (#6256)
* doc(storage): wrong arguments for quickstart program (#6249)
* fix(storage): deflake IAM integration test (#6234)
* fix(storage): disable self-signed JWT authentication (#6230)
* refactor(storage): use g::c::Options for GrpcClient (#6209)
* refactor(storage): use g::c::Options in CurlClient (#6203)
* refactor(storage): compute default options in a single function (#6200)
* refactor(storage): use g::c::Options in ClientOptions (#6183)

### Spanner

* fix(spanner): propagate request_options in ReadPartition serialization (#6319)

### Common libraries

* feat(common): credentials for service account impersonation (#6429)
* feat(common): gRPC support for  service account impersonation (#6401)
* feat(common): support asynchronous RPCs in GrpcAuthenticationStrategy (#6391)
* feat: a cache for access tokens retrieved using gRPC (#6381)
* feat: minimal stub for service account impersonation (#6348)
* feat: support errors in DynamicAccessTokenCredentials (#6325)
* feat(common): support unified credentials in gRPC (#6304)
* feat(common): bootstrap Unified Authentication Client (#6299)
* refactor!: remove gcpcxx.pb suffix from generated files (#6286)
* feat(iam): an example for GenerateAccessToken (#6188)

### Misc.

**BREAKING CHANGES**:
* In #6243 we stopped compiling the code in `generator/` by default in CMake
  builds. In most cases this should just be a performance win as this code is
  not used by client libraries. However, if anyone was relying on the
  `generator/` being compiled, it can be re-enabled with
  `-DGOOGLE_CLOUD_CPP_ENABLE_GENERATOR=ON`
* In #6286, we removed the .gcpcxx.pb suffix from filenames emitted from the C++
  microgenerator. Any usages of the bigquery, iamcredentials, or logging
  experimental libraries will require updating of include paths in user code.

## v1.26.1 - 2021-04

### Storage

* fix(storage): disable self-signed JWT authentication (#6238)

## v1.26.0 - 2021-04

### BigQuery

**NOTE** This release includes an **experimental** BigQuery Storage Read client
library. This library is **NOT GA** and is likely to have breaking changes in
the coming months. Feel free to take a look, file issues, ask questions, and
even experiment with it, but do not ship production code using it yet.

* feature(bigquery): add quickstart (#6116)
* feature(bigquery): add success case integration tests (#6102)
* feature(bigquery): storage library generation (#5989)

### Bigtable:
**BREAKING CHANGES**:
* `bigtable::AdminClient`, `bigtable::DataClient`, and
  `bigtable::InstanceAdminClient` each gained a new, pure virtual
  `BackgroundThreadsFactory()` member function, requiring anyone who derives
  from those classes to implement that function. This would typically only
  affect users who create mock clients for testing purposes.

**OTHER CHANGES**:
* feat(common): experimental logging configuration (#6049)
* feat(bigtable): Bind CompletionQueue with Table (#6012)
* feat: bind `CompletionQueue` with `TableAdmin` (#6004)
* feat: bind `CompletionQueue` with `InstanceAdmin` (#5967)
* feat(bigtable): enable keepalive pings by default (#5969)

### Pub/Sub
* feat(pubsub): schemas are no longer experimental (#6115)
* fix(pubsub): deadlocks for cancel during session startup (#6055)
* feat(common): experimental logging configuration (#6049)
* doc(pubsub): fix region tags for two examples (#5944)

### Storage

* doc(storage): always use signing account in examples (#6149)
* feat(storage): self-signed JWTs for service accounts (#6096)
* doc(storage): better description for ObjectWriteStream (#6075)
* cleanup: deprecate some IAM types (#6069)
* fix(storage): move disabling consts (#6064)
* fix(storage): use after move problems (#6066)
* feat(common): experimental logging configuration (#6049)
* doc(storage): describe how to use optional parameters (#5983)
* fix(storage): correctly set customTime on inserts (#5980)
* fix(storage): do not update upload session ids (#5979)
* feat(storage): implement storage::GrpcClient::TestBucketIamPermissions (#5957)
* feat(storage): implement storage::GrpcClient::DeleteResumableUpload (#5941)

### Spanner

* feature(spanner): request priority (#6103)
* feature(spanner): update CMEK samples (#6120)
* feature(spanner): customer-managed encryption (#6087)
* feat(common): experimental logging configuration (#6049)
* feat(spanner): connection factories now prefer `Options` (#6046)
* feat(common): make Options public (#6042)
* doc(spanner): fix expected option lists (#6029)

### Common libraries

* feat(common): experimental logging configuration (#6049)
* feat(common): log the last N entries (#6048)
* feat(common): make `google::cloud::Options` public (#6042)

## v1.25.0 - 2021-03

### Bigtable:

* We have marked the asynchronous versions of the administrative functions as
  deprecated. These functions were experimental and we do not think they add
  value to our customers. Removing them would simplify the code and free
  development time for further features and cleanups.  More information in
  [#5923].

* The legacy IAM functions were marked as deprecated via doxygen comments.
  Now they should generate warnings at the call site, depending on your
  compiler settings. See [#5929] for more information.

* feat(bigtable): add CMEK attributes to admin APIs (#5921)
* feat(bigtable): limit default connection pool size (#5881)
* doc: workarounds for Bazel and path length problems (#5869)
* doc(bigtable): add configure_connection_pool sample (#5839)

### Pub/Sub:

* Implemented support for "schemas". This feature allows you to define the
  schema of the messages accepted by a `Topic`. The schemas can be defined using
  AVRO or Protocol Buffers. This is a public preview feature of Cloud Pub/Sub,
  the APIs are found in the `::google::cloud::pubsub_experimental` namespace.

### Spanner:

* feat(spanner): Point-In-Time Recovery (PITR) (#5906)
  This is a major new feature in Spanner, supporting backups and restores at
  a given timestamp.
* refactor(spanner): spanner::Timestamp/protobuf::Timestamp conversions… (#5876)
* feat(spanner): statistics returned for a committed transaction (#5809)

### Storage:

* The legacy IAM functions were marked as deprecated via doxygen comments.
  Now they should generate warnings at the call site, depending on your
  compiler settings. See [#5929] for more information.

* doc(storage): Update UBLA documentation to reflect its current status (#5870)
  It has been GA for a long time, but was still described as not in our
  comments.
* fix(storage): remove unneeded dependency (#5798)
  The backwards compatibility target and package (`storage_client`) required
  the googleapis protos.

[iam-conditions-link]: https://cloud.google.com/iam/docs/conditions-overview
[#5923]: https://github.com/googleapis/google-cloud-cpp/issues/5923
[#5929]: https://github.com/googleapis/google-cloud-cpp/issues/5929

## v1.24.0 - 2021-02

**NOTE**:
We are clarifying our approach to backwards compatibility beyond the C++ API.
See the [README.md](README.md) on GitHub for details

### Bazel
* Starting with this release the legacy targets, such as
  `//google/cloud/pubsub:pubsub_client` are deprecated and generate warnings
  recommending a replacement (such as `//:pubsub`). Note that you may have to
  prefix the target with the external package name you gave this library, e.g.,
  `@github_com_google_cloud_cpp//:pubsub`. (#5746)

### Bigtable
* feat(bigtable): restore from backups in other instance (#5754)
* feat(bigtable): better control over channel refresh (#5753)

### Spanner
No user-facing changes.

### Storage
**BREAKING CHANGES**:
* UniformBucketLevelAccess was known as BucketPolicyOnly during the beta. For
  compatibility the C++ Cloud Storage library supported both, however
  BucketPolicyOnly is now completely removed. (#5720)

**Other Changes**:
* feat(storage): Support includeTrailingDelimiter in Client::ListObjects (#5713)

### Common Libraries
**BREAKING CHANGES**:
* refactor!: removed old bigquery code (#5722)

## v1.23.0 - 2021-01

### Common Libraries

* fix: crashes during client shutdown (#5701)

### Bigtable

* Removed [OpenCensus](https://opencensus.io) example. OpenCensus has merged
  with OpenTracing into [OpenTelemetry](https://opentelemetry.io). The C++
  development seems to have stalled, and we not longer believe it will ever
  be mature enough to recommend for the `google-cloud-cpp` libraries.

## v1.22.0 - 2021-01

### Bigtable

* feat(bigtable): enforce id limits in admin emulator (#5679)
* feat(bigtable): create logging layer for Cloud Bigtable DataClient (#5654)
* feat(bigtable): create logging layer for Cloud Bigtable InstanceAdminClient (#5653)
* feat(bigtable): create logging layer for Cloud Bigtable (#5556)
* feat: implement Bigtable connection refresh (#5550)
* feat: Implement backup level IAM policy  (#5585)

### Pub/Sub

* feat(pubsub): add equality to Publisher (#5608)

### Storage

* fix(storage): incorrect CURL handle manipulation (#5651)
* feat(storage): propagate custom header with resumable upload PUT requests (#5632)
* fix: only build gRPC testing utilities if needed (#5594)

### Spanner

* cleanup: rename spanner::internal to spanner_internal (#5620)
* fix: get the code to compile with MSVC 2017 (#5574)

### Common Libraries

* fix: only build gRPC testing utilities if needed (#5594)
* feat: Implement backup level IAM policy  (#5585)
* fix: get the code to compile with MSVC 2017 (#5574)
* cleanup: remove custom C++ version variable (#5674)

## v1.21.0 - 2020-12

**BREAKING CHANGES:**

* Some "Range" types used in the Storage, Pub/Sub and Spanner APIs lost a
  constructor that was never intended to be part of their public APIs. Users
  who were not directly constructing these ranges will not be affected. Also
  some performance improvements were made to their iterator implementations
  that could break callers who were relying on unspecified behavior that is not
  required by the [input
  range](https://en.cppreference.com/w/cpp/named_req/InputIterator) concept.
  The affected types are:
  * `google/cloud/storage/list_buckets_reader.h`
    * `using ListBucketsReader = google::cloud::internal::PaginationRange`
  * `google/cloud/storage/list_hmac_keys_reader.h`
    * `using ListHmacKeysReader = google::cloud::internal::PaginationRange`
  * `google/cloud/storage/list_objects_and_prefixes_reader.h`
    * `using ListObjectsAndPrefixesReader = google::cloud::internal::PaginationRange`
  * `google/cloud/storage/list_objects_reader.h`
    * `using ListObjectsReader = google::cloud::internal::PaginationRange`
  * `google/cloud/pubsub/subscription_admin_connection.h`
    * `using ListSubscriptionsRange = google::cloud::internal::PaginationRange`
    * `using ListSnapshotsRange = google::cloud::internal::PaginationRange`
  * `google/cloud/pubsub/topic_admin_connection.h`
    * `using ListTopicsRange = google::cloud::internal::PaginationRange`
    * `using ListTopicSubscriptionsRange = google::cloud::internal::PaginationRange`
    * `using ListTopicSnapshotsRange = google::cloud::internal::PaginationRange`
  * `google/cloud/spanner/database_admin_connection.h`
    * `using ListDatabaseRange = google::cloud::internal::PaginationRange`
    * `using ListBackupOperationsRange = google::cloud::internal::PaginationRange`
    * `using ListDatabaseOperationsRange = google::cloud::internal::PaginationRange`
    * `using ListBackupsRange = google::cloud::internal::PaginationRange`
  * `google/cloud/spanner/instance_admin_connection.h`
    * `using ListInstancesRange = google::cloud::internal::PaginationRange`
    * `using ListInstanceConfigsRange = google::cloud::internal::PaginationRange`

### Bigtable

* fix(bigtable): missing functions to change policies (#5565)
* feat(bigtable): create logging layer for Cloud Bigtable (#5515)
* docs: document Bigtable thread safety (#5394)

### Pub/Sub

* feat(pubsub): by default Subscriber::Subscribe retries forever (#5552)
* fix(pubsub): deadlock during streaming pull shutdown (#5547)
* fix(pubsub): deadlock during fire & forget shutdown (#5541)
* refactor!: PaginationRange<T> is now an alias to StreamRange<T> (#5538)
* doc(pubsub): declare the library GA (#5390)
* doc(pubsub): how to run throughput benchmark (#5500)
* feat(pubsub): separate subscribers in benchmark (#5499)
* feat(pubsub): separate publishers in benchmark (#5496)
* feat(pubsub): minor optimization in publisher (#5495)
* fix(pubsub): reduce data copying in publish (#5484)
* feat(pubsub): more efficient callback dispatch (#5458)
* feat(pubsub): batch acks and nacks in streams (#5464)
* feat(pubsub): increase default buffer sizes (#5449)
* feat(pubsub): pace publisher in throughput benchmark (#5447)
* feat(pubsub): improve lease management performance (#5454)
* feat(pubsub): more details in throughput benchmark (#5451)
* feat(pubsub): import throughput benchmark reports (#5450)
* feat(pubsub): show ack throughput in benchmark (#5439)
* feat(pubsub): handle AsyncPull responses in batches (#5441)
* feat(pubsub): round robin subscriber channels (#5423)
* feat(pubsub): more throughput benchmark options (#5419)
* fix(pubsub): keep corked after ResumePublish (#5415)
* fix(pubsub): sometimes published oversized batches (#5409)
* feat(pubsub): round robin publisher connections (#5401)
* feat(pubsub): reduce default channel count (#5402)

### Spanner

* feat(spanner): Point-In-Time Recovery (lite) throttled DB update (#5562)
* refactor!: PaginationRange<T> is now an alias to StreamRange<T> (#5538)

### Storage

* refactor!: change gcs to use common PaginationRange<T> (#5545)
* fix(storage): use channel options for credentials (#5524)
* fix: validate service account credentials contain a usable key (#5404)

### Common Libraries

* feat: add value_type to StatusOr<T> (#5535)
* feat: introduce a generic StreamRange<T> (#5532)
* refactor(common): move CompletionQueue mock (#5463)
* feat: a faster CompletionQueue::RunAsync (#5406)

## v1.20.0 - 2020-11

### Bigtable

No user-facing changes.

### Pub/Sub

**BREAKING CHANGES:**

This is the first GA release of the Pub/Sub library. While breaking changes
before GA should be expected, we think highlighting them here is important.

* Simplify the concurrency control configuration in `pubsub::SubscriberOptions`.
  Applications only need to set the maximum number of messages that will be
  scheduled in parallel.

* Remove `pubsub::AckHandler::ack_id()` accessor. We believe application
  developers should have no need for this field.

* Rename `pubsub::SubscriptionOptions` to `pubsub::SubscriberOptions` as these
  are bound to a specific subscriber object.

* Change the `pubsub::Subscriber` API. A `Subscriber` is now bound to a specific
  Cloud Pub/Sub subscription, with a fixed set of `SuscriptionOptions`, just
  like a `pubsub::Publisher` is bound to a specific topic and a set of
  `PublisherOptions`. In addition to making publishers and subscribers more
  symmetrical, this makes the library more consistent with the Cloud Pub/Sub
  library for other languages. Finally, note that we are planning to rename
  `SubscriptionOptions` to `SubscriberOptions` in a future PR too.

* Remove option to disable retries in `Publisher::Publish`. This is redundant as
  the application can set a "no retries" retry policy. This is more consistent
  with other Cloud Pub/Sub libraries. We include an example showing how to
  configure a "no retries" retry policy.

* Fix inconsistent naming for `PublisherOptions` attributes controlling the
  maximum number of messages per batch and the maximum number of bytes per
  batch.

* Rename the `{Topic,Snapshot,Subscription}MutationBuilder` classes, removing
  `Mutation` from their names. This makes the C++ library more familiar for
  Cloud Pub/Sub developers coming from other languages.

* Simplify the message flow control configuration. Now that the library uses
  streaming pulls, the low water marks are not used. The application developer
  simply sets limits for the number of messages (and/or bytes) outstanding.
  These limits are propagated to the service, and the service will stop
  streaming if too many messages (or bytes) are outstanding.

### Spanner

No user-facing changes.

### Storage

**BREAKING CHANGES:**

* Our public headers no longer include `nlohmann/json.hpp`. Please update your
  code to directly include this header if you need it. We believe it is not a
  good practice to depend on indirectly included headers, but do feel we should
  warn our customers of this change.

**OTHER CHANGES:**

* Unexpected curl errors will now be retried (#5312)
* docs: add error handling example from `client->ReadObject()` (#5274)
* feat(storage): Create an example for `Client::DeleteResumableUpload()`
* doc: prefer UBLA references over bucket-policy-only

### Common Libraries

* Fixed occasional crash on background thread shutdown (#5324)
* `GCP_LOG` now serializes its output to `std::clog` (#5179)

## v1.19.0 - 2020-10

### Storage

* fix(storage): consistent computation of XML vs. JSON (#5095)
  The interaction of `ClientOptions::set_endpoint()` and the
  `CLOUD_STORAGE_TESTBENCH_ENDPOINT` environment variable was inconsistent
  across endpoints. For JSON endpoints `set_endpoint()` overrode the
  `CLOUD_STORAGE_TESTBENCH_ENDPOINT` value, while for XML endpoints it
  was the opposite.

  In other libraries the environment variable always wins, so we are changing
  the behavior here. This behavior was never documented, and it was buggy,
  therefore it is not a breaking change. Nonetheless, we think the bug (and
  the fix) is surprising enough to highlight in the CHANGELOG.

* fix: enable `codecvt` (UTF-8 support for signed URLs) in MSVC (#5126)
* fix(storage): avoid stalls with small reads (#5104)
* feat(storage): disable XML via environment variable (#5100)
* fix(storage): use correct host header (#5085)

### Spanner

* fix: retry gRPC operations if the connection is unexpectedly terminated (#5087)
* doc(spanner): use custom retry example in the Doxygen docs (#5164)

### Pub/Sub

> :warning: This library is under development and subject to breaking
> changes without notice.

* feat(pubsub): wait for callback return *and* handler (#5161)
* In addition, @remyabel contributed a number of examples.

## v1.18.0 - 2020-09

### Storage

* fix: add missing object ACLs in gRPC client (#5029)
* fix: work with unknown SSL version in curl (#5037)

### Spanner

* This release includes support for the `NUMERIC` data type in Cloud Spanner.
* doc: adapt to new specification for NUMERIC samples (#5049)
* doc: add `spanner_query_with_*_parameter` samples (#5016)
* feat: start using NUMERIC types in database schema (#5025)

### Pub/Sub

* The current release is a preview of the upcoming GA release. While we think the APIs are unlikely to change before GA, we reserve the right to change these APIs for now. This release may need optimization before it is ready for production workloads.

### Common Libraries

* feat: create promises without shared state (#5046)
* feat: make CompletionQueueImpl mockable [1] (#5036) (#5039) (#5043)
* feat: support cancels for asynchronous unary RPCs (#5047)
* fix: correct C++ version under MSVC (#5038)
* fix: CompletionQueue shutdown disables RunAsync (#5008)
* fix: remove 'Bigtable' from generic error message (#5034)
* fix: remove unneeded dep on absl::variant (#5054)

## v1.17.0 - 2020-09

### Bigtable

* doc: update quickstart README files (#4980)

### Storage

**BREAKING CHANGES**

* fix(storage)!: use nlohmann_json library as any other dependency (#4747)
  * After this change applications using CMake must install the
  [nlohmann_json][nlohmann-json-gh] library. Automatically downloading
  the library (a) creates problems for package maintainers, (b) requires
  brittle code to keep the symbols from leaking, and (c) creates problems
  for users that need newer versions of this library, for example, because
  they are using a compiler that was not supported by the version we pick.
    * Applications using Bazel or CMake super builds should not be impacted.
    * We have updated our instructions to install this library from source on
     multiple Linux distributions.
    * Applications using a package manager will need to update their build
    scripts to add this dependency.
  * In addition, this removes a number of symbols in the
  `google_cloud_cpp_internal_nlohmman_json_3_4_0::` namespace.
  Obviously we never intended these symbols for public use, but we
  should have been clearer about it.

[nlohmann-json-gh]: https://github.com/nlohmann/json.git

**Other Changes**

* doc: update quickstart README files (#4980)
* feat(storage): custom_time in ObjectMetadata (#4901)
* feat(storage): noncurrent and custom time OLM (#4871)
* fix: cmake configs missing find_dependency(abseil) (#4919)

### Spanner

* doc(spanner): add samples for "datatypes" region tags (#4999)
* doc: update quickstart README files (#4855) (#4980)
* fix(spanner): correct variable used to build instance config (#4791)

### Common Libraries

* feat: add a `KmsKeyName` class (#4891)
* feat: automatically create background thread pools (#4936)
* fix: clang-tidy works on all headers (#4844)
* fix: cmake configs missing find_dependency(abseil) (#4919)

## v1.16.0 - 2020-08

### Bigtable

* doc: remove not GA warning from bigtable backup methods (#4601)
* doc: restore quickstart's region tags (#4525)
* fix: builds on 64-bit Windows (#4523)
* fix: warnings under Windows+MSVC+x86 (#4515)
* feat: `CompletionQueue::RunAsync` with no arguments (#4450)

### Storage

**BREAKING CHANGES**
* fix!: incorrect type in OLM condition (#4597)\
  **changed the `created_before` field in `LifecycleRuleCondition` from a time
   point to a date**

**Other Changes**
* fix: GCE credentials response handling (#4739)
* feat: add a function to delete resumable upload in client (#4696)
* doc: include guidelines for bucket names (#4688)
* feat: add a function `DeleteResumableUpload` to `RawClient` (#4678)
* fix: missing `CreateDefaultClientOptions` overload (#4677)
* feat: a request type to delete pending resumable uploads (#4617)
* feat: disable MD5Hash by default (#4591)
* feat: Add startOffset and endOffset (#4518)
* fix: warnings under Windows+MSVC+x86 (#4515)
* fix: avoid chunked transfer encoding (#4506)
* fix: avoid unnecessary chunk to finalize uploads (#4504)
* feat: Add `Client::ListObjectsAndPrefixes` (#4494)
* fix: multiple problems under Windows+MSVC+x86 (#4502)
* feat: reduce data copies in uploads (#4496)
* fix: workaround min/max macros on Windows (#4490)
* feat: reduce copies during uploads (#4489)
* feat: Add `UploadFromOffset` and `UploadLimit` to `UploadFile` (#4473)
* feat: HTTP requests with many buffers (#4480)
* feat: Add `value_or()` to options params and headers (#4408)

### Spanner

**BREAKING CHANGES**
* refactor!: `spanner::Timestamp` implementing using `absl::Time` (#4625)\
  **spanner::Timestamp no longer allows construction from or conversion to
   a chrono time point with picosecond precision**
* refactor!: use `absl::CivilDay` for "date" type (#4600) \
  **`absl::CivilDay's` constructors are explicit (by design), where as the old
  `spanner::Date` 3-arg constructor was not explicit.**

**Fix issue #4516: atomicity is violated if the first operation in a RW
  Transaction fails.**\
  If this occurs, the library now explicitly begins a new transaction and
  retries the failed operation. This ensures subsequent operations using the
  same `Transaction` object are in fact executed in the same Spanner
  transaction.
* fix: begin a `has_begin` transaction in Rollback() (#4731)
* fix: handle `ExecuteBatchDml` returning OK with no results (#4724)
* fix: use the updated transaction id in `ReadImpl` (#4722)
* fix: explicitly begin failed implicit begin transactions (#4706)
* feat: handle invalid transactions in `ConnectionImpl` methods
* feat: store a Status when invalidating Transactions (#4670)
* feat: support moving a `Transaction` into an error state (#4545)

**Other Changes**
* doc: note that NUMERIC columns are not yet available (#4738)
* feat: add `google-cloud-resource-prefix` to non-admin operations (#4703)
* fix: date formatting with negative years (#4614)
* feat: add `google::cloud::spanner::Numeric` (#4418)
* fix: warnings under Windows+MSVC+x86 (#4515)
* feat: log the result for `future<StatusOr<T>>` (#4746)

### Common libraries

**BREAKING CHANGES**
* fix!: incorrect type in OLM condition (#4597)\
  **changed the `created_before` field in `LifecycleRuleCondition` from a time
   point to a date**

**Other Changes**
* fix: timestamp proto encoding works before the epoch and with extreme values (#4611)
* feat: `CompletionQueue::RunAsync` with no arguments (#4450)
* feat: log exceptions in example driver (#4453)

## v1.15.0 - 2020-07

### Bigtable

* feature: add bigtable backup API support (#4407)
* fix: deadlock in MutationBatcher (#4327)

### Storage

* fix: support ObjectReadStream::tellg() (#4402)
* fix(storage): treat 408 errors as retryable (#4397)
* fix(GCS+gRPC): simplify DirectPath configuration (#4388)
* feat(GCS+gRPC): DirectPath can be manually configured (#4379)
* fix: warnings with MSVC 2019 16.6 (aka 19.26 akak 14.26) (#4365)
* feat(storage/benchmarks): compare to raw downloads (#4362)
* feat(storage/benchmark): experiment integration test (#4360)
* feat(storage/benchmarks): control CRC32C/MD5 options (#4326)

### Spanner

* fix: incorrect metadata key string (#4431)
* doc(spanner): add CreateInstance() example (#4405)
* fix(spanner): examples on how to delete data (#4401)
* feat: use `SELECT 1` to refresh sessions (#4377)
* fix(spanner): use correct name for test (#4373)

### Common libraries

* fix: CompletionQueue::RunAsync is always async (#4448)
* fix: broken builds on Windows+CMake+Release (#4442)
* fix: test with correct MockCompletionQueue (#4427)
* fix: make potential narrowing cast of nanoseconds explicit (#4391)
* fix: memory stomping in CompletionQueue::RunAsync (#4330)

## v1.14.0 - 2020-06

### General Notices

* This is the first release that includes the Spanner library, which previously
  lived at github.com/googleapis/google-cloud-cpp-spanner. All future releases
  of the Spanner library will be from this repo, and the other repo will be
  archived.
* In this release we take a dependency on the [Abseil]
  (https://github.com/abseil/abseil-cpp) C++ library.
* In this release we _dropped_ our dependency on
  https://github.com/googleapis/cpp-cmakefiles. We moved the CMake rules to
  compile and install the googleapis protos into this repo in the
  `external/googleapis/` directory.

### Bigtable

* feat: more bigtable data filter samples (#4315)
* feat: add rvalue reference overloads to `Row` (#4239)
* feat: implemented an efficient `SetCell(Cell)` overload to copy an existing cell to a mutation
* feat: remove dep on cpp-cmakefiles, integrating the CMake rules into this repo (#4245)
* feat: add absl crash handler support for bigtable examples (#4150)
* feat: more bigtable data filter samples (#4141)
* fix: warning options exported in library interface (#4134)
* fix: proper routing headers for longrunning ops (#4099)
* feat: add bigtable data filter samples (#4069)
* fix: bigtable's random_mutation_test missing from build (#4058)
* chore: bigtable, storage quickstarts use top-level build targets (#4050)

### Storage

* feat: support X-Upload-Content-Length header (#4284)
* feat(storage/benchmark): cleanup storage benchmarks
* feat(storage/benchmark): support GCS+gRPC plugin in storage benchmarks
* feat: **EXPERIMENTAL** introduced an optional gRPC plugin to the GCS client library.
* feat: support XML vs. JSON in throughput_vs_cpu_benchmark (#4277)
* fix: fix off-by-one in uploading streams to GCS (#4250)
* feat: proto conversions for BucketAccessControl (#4247)
* feat: remove dep on cpp-cmakefiles, integrating the CMake rules into this repo (#4245)
* feat: implement To/FromProto for CustomerEncryption (#4242)
* fix: warning options exported in library interface (#4134)
* fix: C2593 'operator =' is ambiguous (#4059)
* chore: bigtable, storage quickstarts use top-level build targets (#4050)

### :star2: Spanner

* This is the first release of this repo that contains the Cloud Spanner C++
  Client Library. Previously, this library lived in a separate repo
  (https://github.com/googleapis/google-cloud-cpp-spanner). That old repo will
  be archived, and all future Cloud Spanner C++ Client Library releases will
  come from this repo.

### Common libraries

* feat: remove dep on cpp-cmakefiles, integrating the CMake rules into this repo (#4245)
* fix: warning options exported in library interface (#4134)
* fix: use correct variable for SOVERSION (#4131)

## v1.13.0 - 2020-05

**NOTICE:** This release aligns all the version numbers for Bigtable, Storage,
the common libraries, and the repository itself. From this point on all
releases will include a single version number. Note that there is a gap in the
version numbers for Bigtable and the repository changes from v0.21.0 to
v**1**.13.0

**BREAKING CHANGES**

* The common library inlined namespace changed to `v0` to `v1`, moving all the
  symbols from `google::cloud::v0` to `google::cloud::v1`. Applications that
  explicitly referred to the inlined namespace would be affected by this change.
  That is, code that uses `google::cloud::v0::Foo` instead of
  `google::cloud::Foo` would need to be modified. We apologize if this has
  impacted you, and recommend that applications do not refer to the inlined
  namespace in the future.

### Bigtable

**BREAKING CHANGES**

See above regarding the common library inlined namespace.

**OTHER CHANGES**

* cleanup: the library now links against the common libraries included in this
  repository, applications developers should not install the standalone common
  libraries in the (now archived) google-cloud-cpp-common repository.
* feat: compile bigtable benchmarks with Bazel (#3884)
* feat: implement table level IAM policy for bigtable(async) (#3829)
* feat: implement table level IAM policy for bigtable (#3751)
* refactor: synchronize version numbers for all libraries (#3710)
* feat: change READMEs and quickstart programs (#3690)
  The `google/cloud/bigable/quickstart/` directory contains a sample project
  for CMake and Bazel that uses the Cloud Bigtable C++ client library as we
  expect application developers would.

### Storage

**BREAKING CHANGES**

See above regarding the common library inlined namespace.

* fix!: use correct type for generation numbers (#3870) This changes the type of
  two fields from `google::cloud::optional<long>` to
  `google::cloud::optional<std::int64_t>`. With MSVC this is not a backwards
  compatible change (storing the result of `.value()` into a `long` now loses
  precision). However, on that platform the fields were not usable, they could
  not store the values that are typically generated by the production
  environment. We apologize to any customers affected by this change, but we
  think it is unlikely that anybody used the field on that platform.

**OTHER CHANGES**

* cleanup: the library now links against the common libraries included in this
  repository, applications developers should not install the standalone common
  libraries in the (now archived) google-cloud-cpp-common repository.
* feat: implement POST policy signatures. Applications can use the Google Cloud
  Storage C++ client library to create
  [POST objects](https://cloud.google.com/storage/docs/xml-api/post-object),
  which allow uploading using HTML forms.
* feat: change READMEs and quickstart programs (#3690)
  The `google/cloud/bigable/quickstart/` directory contains a sample project
  for CMake and Bazel that uses the Cloud Bigtable C++ client library as we
  expect application developers would.
* fix: use network to check if running inside Google (#3959) With this change
  applications running on Google App Engine Flex (GAEF), Google Kubernetes
  Engine (GKE), or Cloud Run can use the Google Default Credentials.
* fix: build problems on MSVC x86 (#3916)
* fix: do not use CURL_SHARE features (#3860)
* chore: upgrade libcurl to v7.69.1 (#3851)

### Common Libraries

**BREAKING CHANGES**

See above regarding the common library inlined namespace.

**OTHER CHANGES**

No other interesting changes in the common libraries with this release.

## v0.21.0 - 2020-04

> **NOTICE:** This repo will soon contain the code for all the other related
`google-cloud-cpp-*` repos. As a new monorepo
([#3612](https://github.com/googleapis/google-cloud-cpp/issues/3612)), the
versioning of this repo will be changing to have a single per-repo version.
**The per-library version numbers will be removed in favor of the repo
version.** See https://github.com/googleapis/google-cloud-cpp/issues/3615 for
more info.

### Bigtable (v1.9.x)

**BREAKING CHANGES**

* fix!: moved IAM-related symbols to the correct inlined namespace (#3453)
 Most users should not notice any difference, but those that explicitly referenced
 symbols through the `google::cloud::bigtable::v0` namespace may need to switch
 to `google::cloud::bigtable` (the recommended approach) or
`google::cloud::bigtable::v1`. We apologize if this causes you inconvenience.

### Storage (v1.12.x)

**BREAKING CHANGES**

* fix!: moved IAM-related symbols to the correct inlined namespace (#3453)
 Most users should not notice any difference, but those that explicitly referenced
 symbols through the `google::cloud::storage::v0` namespace may need to switch
 to `google::cloud::storage` (the recommended approach) or
`google::cloud::storage::v1`. We apologize if this causes you inconvenience.

**Other Changes:**

* feat: add support for iam conditions (#3497)
* bug: express libcurl version in hex (#3487)
* bug: check curl connection before unpausing (#3485)
* feat: allow domain named buckets in signed URLs v4 (#3463)
* feat: implement virtual hostname V4 signatures
* feat: add configuration options to set the SSL root of trust (#3455)
* doc: add doxygen comments for ParallelUploadFile (#3448)
* feat: support x-goog-content-sha256 for V4 signed URLs (#3435)

**Other Changes**

* None

## v0.20.0 - 2020-03

### Bigtable (v1.8.x)

* No changes to the Bigtable client in this release.

### Storage (v1.11.x)

* feat: implement resumable parallel file uploads (#3389) - allow applications
  to use multiple threads to upload large files, achieving throughput in excess
  of 1GiB/s. With this change applications can restart such uploads even if the
  application restarts.
* chore: upgrade testbench to Python 3 (#3402) - you will need to have Python 3
  installed to run the integration tests.

## v0.19.0 - 2020-02

### Bigtable (v1.7.x)

* fix: correct environment handling in client_options_test (#3374)
* chore: update g-c-cpp-common to v0.19.0 (#3384)
* chore: upgrade to cpp-cmakefiles v0.4.1 (#3372)

### Storage (v1.10.x)

* chore: update g-c-cpp-common to v0.19.0 (#3384)
* feat: implement ParallelUploadPersistentState (#3382)
* refactor: create ParallelUploadStateImpl (#3379)
* refactor: ScopedDeleter doesn't require metadata (#3380)

## v0.18.0 - 2020-02

### Bigtable (v1.7.x)

* docs: add examples of different credential classes for bigtable
* feat: use Bigtable direct path if configured (#3338)

### Storage (v1.9.x)

**BREAKING CHANGE**

* fix!: insert logging layer only if requested (#3349)

**Other Changes:**

* fix: handle completed uploads in UploadChunk (#3348)
* fix: use ::testing::TempDir for test files (#3345)
* fix: remove debug log from benchmark output (#3341)
* test: allow specifying dir in parallel upload BM (#3339)
* feat: generate plots for parallel upload benchmark (#3336)
* doc: some fixes for storage client docs (#3332)
* feat: improve storage_throughput_benchmark (#3330)
* cleanup: remove tmpnam (#3325)
* test: don't build benchmarks without tests (#3322)
* feat: implement Delimiter option for ListObjects (#3320)
* docs: document parallel uploads design (#3321)
* test: implement a benchmark for parallel uploads (#3302)
* fix: handle CURLE_GOT_NOTHING as retryable (#3316)
* feat: benchmark reading many small chunks (#3313)

## v0.17.0 - 2020-01

### Bigtable (v1.6.x)

*  feat: new functions to create `Chain` and `Interleave` filters from ranges
   of `Filter` objects

### Storage (v1.8.x)

**BREAKING CHANGE**

* feat: move creating prefix marker to ComposeMany (#3306)

**Other Changes:**

* feat: implement parallel uploads. For large files this can improve the upload
  bandwidth by a factor of 20. Note that in this release such uploads cannot be
  resumed after a program restart. (#3279)
* feat: support optionsRequestedPolicyVersion query parameter in
* fix: setting options in requests accepts crefs (#3287)

## v0.16.0 - 2019-12

### Bigtable (v1.5.x)

* feat: implement Bigtable sync vs. async benchmark (#3276)
* fix: detect duplicate cluster ids in `bigtable::InstanceConfig` (#3262)
* bug: use `CalculateDefaultConnectionPoolSize` in `set_connection_pool_size` (#3261)

### Storage (v1.7.x)

* fix: add logic to ObjectWriteStreambuf for handling jumps in upload ranges to fix #3280 (#3283)
* bug: fix error messages in resumable sessions (#3263)

## v0.15.0 - 2019-11

### Build

**BREAKING CHANGE**:

* The common libraries have been moved to the [google-cloud-cpp-common
  repository][github-cpp-common]. While this may not be a technically breaking
  change (the API and ABI remain unchanged, the include paths are the same), it
  will require application developers to change their build scripts.
* Submodule builds no longer supported.

### Dependency updates

* Upgraded cmake-format to 0.6.0. #3211
* Upgraded gRPC to 1.24.3 #3217
* Upgraded googletest to 1.10.0 #3201

### Bigtable (v1.4.x)

* Pass along error message in `Table::Apply` retry loop #3208

### Storage (v1.6.x)

* Implement `ComposeMany` to efficiently compose more than 32 GCS objects #3016
* Implement a function to delete all the objects that match a given prefix #3016
* Support uniform bucket level access #3186
* Use separate policy instances for each UploadChunk request #3213

[github-cpp-common]: https://github.com/googleapis/google-cloud-cpp-common

## v0.14.0 - 2019-10

### Bigtable (v1.3.x)

* bug: fix runtime install directory (#3063)

### Common (v0.12.x)

* bug: fix runtime install directory (#3063)

### Storage (v1.5.x)

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

## v0.13.0 - 2019-09

### Bigtable (v1.2.x)

* feat: Configure C++17 build. (#2961)
* fix: use MetadataUpdatePolicy::FromClusterId. (#2968)
* fix: correct invalid routing headers. (#2988)

### Storage (v1.4.x)

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

### Common (v0.11.x)

* feat: Use macros for compiler id and version (#2937)
* fix: Fix bazel build on windows. (#2940)
* chore: Keep release tags in master branch. (#2963)
* cleanup: Use only find_package to find dependencies. (#2967)
* feat: Add ability to disable building libraries (#3025)
* bug: fix builds with CMake 3.15 (#3033)
* feat: Document behavior of passing empty string to SetEnv on Windows (#3030)

## v0.12.0 - 2019-08

### Bigtable (v1.1.x)

* feat: Minimize contention in Bigtable Client initialization. (#2923)
* feat: Support setting 64-bit integers mutations. (#2866)
* feat: Implement support for IAM conditions. (#2854)
* **BREAKING CHANGE**: use cmake files from
  github.com/googleapis/cpp-cmakefiles for googleapis protos (#2888)

### Storage (v1.3.x)

* feat: Control TCP memory usage in GCS library. (#2902)
* feat: Make partial errors/last_status available to `ObjectWriteStream` (#2919)
* feat: Change storage/benchmarks to compile with exceptions disabled. (#2916)
* feat: Implement native IAM operations for GCS. (#2900)
* feat: Helpers for PredefinedDefaultObjectAcl. (#2885)
* bug: Fix ReadObject() when reading the last chunk. (#2864)
* bug: Use correct field name for MD5 hash. (#2876)

### Common (v0.10.x)

* feat: add `conjunction` metafunction (#2892)
* bug: Fix typo in testing_util/config.cmake.in (#2851)
* bug: Include 'IncludeGMock.cmake' in testing_util/config.cmake.in (#2848)

## v0.11.0 - 2019-07

### Bigtable (v1.0.0)

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

### Storage (v1.2.0)

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

## v0.10.0 - 2019-06

### Bigtable (v0.10.0)

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

### Storage (v1.1.0)

* Implemented option to read an object starting from an offset.
* Automatically restart downloads on error. With this change the download is
  restarted from the last received byte, and using the object generation used
  in the original download as well. (#2693)
* Bugfixes:
  * Service account credentials not refreshing properly. (#2691)
  * Fix build for macOS+CMake. (#2698)
  * WriteObject now supports empty streams. (#2714)

## v0.9.0 - 2019-05

### Bigtable (v0.9.0)

* **Breaking Changes**
  * Return `google::cloud::future` from `InstanceAdmin` functions: this is
    more consistent with all other functions returning futures.
  * Remove unused `bigtable::GrpcError`: the library no longer raises this
    exception, any code trying to catch the exception should be modified to
    handle errors via `StatusOr<T>`.
  * Remove Snapshot-related functions, tests, examples, etc.: this is
    functionality in Cloud Bigtable was controlled by an allow-list, and it is
    no longer expected to reach GA.
* Continue to implement more async APIs (`Async*()` methods) for the
  `InstanceAdmin`, `TableAdmin`, and `Table` classes
  (**Note: These are not yet stable**)
* Bugfixes:
  * Need `ignore_warnings` to actually delete an AppProfile.
  * Fix portability/logical errors in shell scripts.
  * Fix a race condition in `MutationBatcher`.
* Implemented a number of previously missing code samples.

### Common (v0.7.0)

* Support move-only callables in `future<T>`
* Avoid `std::make_exception_ptr()` in `future_shared_state_base::abandon()`.

### Storage (v1.0.0)

* Declared GA and updated major number.
* Support signed policy documents.
* Support service account key files in PKCS#12 format (aka `.p12`).
* Support signing URLs and policy documents using the SignBlob API, this is
  useful when using the default service account in GCE to sign URLs and policy
  documents.

## v0.8.1 - 2019-04

### Bigtable (v0.8.x)

* Use SFINAE to constrain applicability of the BulkMutation(M&&...) ctor.

### Common

* Avoid std::make_exception_ptr() when building without exceptions.

## v0.8.0 - 2019-04

### Bigtable (v0.8.x)

* **Breaking change**: `Table::BulkApply` now returns a
  `std::vector<FailedMutation>` instead of throwing an exception.
* In the future we will remove all the `google::cloud::bigtable::noex::*`
  classes. We are moving the implementation to `google::cloud::bigtable::*`.
* Continuing to implement more async APIs (Note: These are not yet stable):
  * `InstanceAdmin::AsyncDeleteInstance`
  * `Table::AsyncCheckAndMutateRow`
  * `TableAdmin::AsyncDeleteTable`
  * `TableAdmin::AsyncModifyColumnFamilies`
* `BulkMutator` now returns more specific errors instead of generic UNKNOWN.
* Improved install instructions, which are now tested with our CI builds.
* CMake-config files now work without `pkg-config`.
* Removed the googleapis submodule. The build system now automatically
  downloads all deps.
* No longer throw exceptions from `ClientOptions`.

### Common

* Removed the googleapis submodule. The build system now automatically
  downloads all deps.

### Storage (v0.6.x)

* Added initial support for HMAC key-related functions.
* Added support for V4 signed URLs.
* Improved install instructions, which are now tested with our CI builds.
* CMake-config files now work without `pkg-config`.
* No longer throw exceptions from `ClientOptions`.
* Handle object names with slashes.
* Added `ObjectMetadata::set_storage_class`
* Added support for policy documents.

## v0.7.0 - 2019-03

### Bigtable (v0.7.x)

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

### Common

* **Breaking change**: Make `google::cloud::optional::operator bool()` explicit.
* Add `google::cloud::optional` value conversions that match `std::optional`.
* Stop using grpc's `DO_NOT_USE` enum.
* Remove ciso646 includes to force traditional spellings.
* Change `std::endl` -> `"\n"`.
* Enforce formatting of `.cc` files.

### Storage (v0.5.x)

* Properly handle subresources in V2 signed URLs.
* Allow specifying non-default `ServiceAccountCredentials` scope and subject.
* Add `make install` instructions.
* Change the storage examples to throw a `std::runtime_error` on failure.
* Add Bucket Policy Only samples.

## v0.6.1 - 2019-02

### Bigtable (v0.6.x)

* No changes from v0.6.0

### Common

* No changes from v0.6.0

### Storage (v0.4.x)

* The library is now **Beta**. We no longer expect changes to the API.
* No other changes from v0.6.0

## v0.6.0 - 2019-02

### Bigtable (v0.6.x)

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

### Common

* Fixed some documentation.
* **Breaking change**: Removed `StatusOr<void>`.
* Updated `StatusOr` documentation.
* Fixed some (minor) issues found by coverity.

### Storage (v0.4.x)

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

## v0.5.0 - 2019-01

### Bigtable (v0.5.x)

* Restore support for gcc-4.8.
* @remyabel cleaned up some hard-coded zone names in the examples.
* More experimental asynchronous APIs, including AsyncReadRows. Note that we
  expect to change all these experimental APIs as described in
  [#1543](https://github.com/GoogleCloudPlatform/google-cloud-cpp/issues/1543).
* @remyabel contributed changes to disable the unit and integration tests. This
  can be useful for package maintainers.
* New Bigtable filter wrapper that accepts a single column.
* **Breaking Change**: remove the `as_proto_move()` member functions in favor
  of `as_proto() &&`. With the latter newer compilers will warn if the object
  is used after the destructive operation.

### Common

* Support compiling with gcc-4.8.
* Fix `GCP_LOG()` macro so it works on platforms that define a `DEBUG`
  pre-processor symbol.
* Use different PRNG sequences for each backoff instance, previously all the
  clones of a backoff policy shared the same sequence.
* Workaround build problems with Xcode 7.3.

### Storage (v0.3.x)

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

## v0.4.0 - 2018-12

### Bigtable (v0.4.x)

* More experimental asynchronous APIs, note that we expect to change all these
  experimental APIs as described in #1543.
* Most of the admin operations now have asynchronous APIs.
* All asynchronous APIs in `noex::*` return an object through which applications
  can request cancellation of pending requests.
* Prototype asynchronous APIs returning a `google::cloud::future<T>`,
  applications can attach callbacks and/or block on a
  `google::cloud::future<T>`.

### Common

* Implement `google::cloud::future<T>` and `google::cloud::promise<T>` based on
  ISO/IEC TS 19571:2016, the "C++ Extensions for Concurrency" technical
  specification, also known as "futures with continuations".

### Storage (v0.2.x)

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

## 0.4.0-pre1 - 2018-12

### Bigtable (v0.4.x)

* More experimental asynchronous APIs, note that we expect to change all these
  experimental APIs as described in #1543.
* Most of the admin operations now have asynchronous APIs.
* All asynchronous APIs in `noex::*` return an object through which applications
  can request cancellation of pending requests.
* Prototype asynchronous APIs returning a `google::cloud::future<T>`,
  applications can attach callbacks and/or block on a
  `google::cloud::future<T>`.

### Common

* Implement `google::cloud::future<T>` and `google::cloud::promise<T>` based on
  ISO/IEC TS 19571:2016, the "C++ Extensions for Concurrency" technical
  specification, also known as "futures with continuations".

### Storage (v0.2.x)

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

## v0.3.0 - 2018-11

This is the v0.3.0 release of the Google Cloud C++ Client Libraries.

### Bigtable (v0.3.0)

* Experimental asynchronous APIs.
* Include an example that illustrates how to use OpenCensus and the Cloud
  Bigtable C++ client library.
* Several cleanups around dependency management with CMake.
* Jason Zaman contributed improvements and fixes to support soversion numbers
  with CMake.
* Lots of improvements to the code coverage in the examples and tests.
* Fixed multiple documentation issues, including a much better landing page
  in the Doxygen documentation.

### Common

* `google::cloud::optional<T>` an intentionally incomplete implementation of
  `std::optional<T>` to support C++11 and C++14 users.
* Applications can configure `google::cloud::LogSink` to enable logging in some
  of the libraries and to redirect the logs to their preferred destination.
  The libraries do not enable any logging by default, not even to `stderr`.
* `google::cloud::SetTerminateHandler()` allows applications compiled without
  exceptions, but using the APIs that rely on exceptions to report errors, to
  configure how the application terminates when an unrecoverable error is
  detected by the libraries.

### Storage (v0.1.x)

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

## v0.3.0-pre1 - 2018-11

### Bigtable (v0.3.0)

* Experimental asynchronous APIs.
* Include an example that illustrates how to use OpenCensus and the Cloud
  Bigtable C++ client library.
* Several cleanups around dependency management with CMake.
* Jason Zaman contributed improvements and fixes to support soversion numbers
  with CMake.
* Lots of improvements to the code coverage in the examples and tests.
* Fixed multiple documentation issues, including a much better landing page
  in the Doxygen documentation.

### Common

* `google::cloud::optional<T>` an intentionally incomplete implementation of
  `std::optional<T>` to support C++11 and C++14 users.
* Applications can configure `google::cloud::LogSink` to enable logging in some
  of the libraries and to redirect the logs to their preferred destination.
  The libraries do not enable any logging by default, not even to `stderr`.
* `google::cloud::SetTerminateHandler()` allows applications compiled without
  exceptions, but using the APIs that rely on exceptions to report errors, to
  configure how the application terminates when an unrecoverable error is
  detected by the libraries.

### Storage (v0.1.x)

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

## v0.2.0 - 2018-08

This is the v0.2.0 release of the Google Cloud C++ Client Libraries.

### Bigtable (v0.2.0)

* Status: **Beta**.
* All data manipulation and admin APIs are implemented.
* All APIs have documentation and short examples showing how to use them.
* The API is not expected to change before 1.0

## v0.1.0 - 2018-03

This is the v0.1.0 release.

### Bigtable (v0.1.0)

* Status: **Alpha**.
* All synchronous APIs for data manipulation and for table administration are implemented.
* All APIs have integration tests and short examples.

## v0.1.0-pre2 - 2018-03

This is the second pre-release of v0.1.0, to further refine the process.

### Bigtable (v0.1.0)

* All synchronous APIs for data manipulation and table administration are implemented.
* We want to add better examples and additional unit tests to wrap up v0.1.0.

## v0.1.0-pre1 - 2018-03

This release is mainly created so we can fine tune the process of creating
releases.  The relevant notes are:

### Bigtable (v0.1.0)

* Synchronous API for data operations largely complete, only `SampleRowKeys()`
  and `ReadModifyWrite()` are missing.
* Synchronous API for table admin operations is complete.
