# Changelog

**NOTE**: [Future Breaking Changes](/doc/deprecated.md) lists anticipated
breaking changes in the upcoming 4.x release. This release is scheduled for
2027-12 or 2028-01.

**NOTE**: Please refer to the [V3 Migration Guide](/doc/v3-migration-guide.md) 
for details on updating existing applications using v1.x.y or v2.x.y.

## v3.7.0 - 2026-07

### Removed Libraries

- The `pubsublite` client library has been removed because the Pub/Sub Lite service has been turned down.

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Agent Registry API](/google/cloud/agentregistry/README.md)

### [Bigtable](/google/cloud/bigtable/README.md)

- Explicit instance declaration is now encouraged during client initialization via a new overload of `MakeDataConnection` that takes a `std::vector<InstanceResource>`. Specifying the target instances at client startup enables optimizing connection pooling (pre-warming/priming channels) and telemetry. The experimental `InstanceChannelAffinityOption` has been removed; the new `MakeDataConnection` overload should be used instead.

  ```cpp
  #include "google/cloud/bigtable/data_connection.h"

  namespace cbt = ::google::cloud::bigtable;

  auto connection = cbt::MakeDataConnection(
      {cbt::InstanceResource(google::cloud::Project("my-project"), "my-instance")},
      google::cloud::Options{});
  ```

- fix(bigtable): safely handle error states in metadata retrieval ([#16225](https://github.com/googleapis/google-cloud-cpp/pull/16225))
- cleanup(bigtable)!: remove experimental InstanceChannelAffinityOption ([#16213](https://github.com/googleapis/google-cloud-cpp/pull/16213))
- feat(bigtable): add explicit instance MakeDataConnection ([#16191](https://github.com/googleapis/google-cloud-cpp/pull/16191))
- feat(bigtable): Add support for adding tags to an Instance in InstanceConfig ([#16014](https://github.com/googleapis/google-cloud-cpp/pull/16014))
- fix(bigtable): DynamicChannelPoolSizingPolicyOption used correctly ([#16188](https://github.com/googleapis/google-cloud-cpp/pull/16188))

### [Storage](/google/cloud/storage/README.md)

- feat(storage): Add full object checksum validation for appendable uploads finalize ([#16245](https://github.com/googleapis/google-cloud-cpp/pull/16245))
- fix(storage): enforce mutual exclusion between Close() and Finalize() in async writers ([#16211](https://github.com/googleapis/google-cloud-cpp/pull/16211))
- fix(storage): add Close() support to AsyncWriterConnectionResumed ([#16163](https://github.com/googleapis/google-cloud-cpp/pull/16163))

### [Google APIs interface definitions](https://github.com/googleapis/googleapis)

- This release is based on definitions as of [2026-06-23T06:02:24-07:00](https://github.com/googleapis/googleapis/tree/b6f9ff05aaec18070232a1ab36da98e684bc7909)

## v3.6.0 - 2026-06

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Data Analytics API with Gemini](/google/cloud/geminidataanalytics/README.md)

### Updated Libraries

- [Network Security API] has been updated with several additional services ([#16122](https://github.com/googleapis/google-cloud-cpp/pull/16122)) 

### Auth

- [Google Distributed Cloud Hosting (GDCH) Service Account credentials](https://docs.cloud.google.com/distributed-cloud/hosted/docs/latest/gdcag/platform/pa-user/service-identity)
  are now supported for REST endpoints. Support for gRPC endpoints will be
  available in a future release.

### [Storage](/google/cloud/storage/README.md)

- fix(storage): avoid premature flush completion and concurrent writes in AsyncWriterConnectionBuffered ([#16169](https://github.com/googleapis/google-cloud-cpp/pull/16169))
- feat(storage): implement GCS overrun logging and resume prevention ([#16150](https://github.com/googleapis/google-cloud-cpp/pull/16150))
- fix(storage): add Close() support to AsyncWriterConnectionBuffered ([#16145](https://github.com/googleapis/google-cloud-cpp/pull/16145))
- fix(storage): Set the idempotency token for async rewrites ([#16114](https://github.com/googleapis/google-cloud-cpp/pull/16114))
- fix(storage): Add telemetry tracing support for async stream Close() ([#16118](https://github.com/googleapis/google-cloud-cpp/pull/16118))
- feat(storage): Add full object read checksum validation for Open ([#16120](https://github.com/googleapis/google-cloud-cpp/pull/16120))
- fix(storage): use server-reported size and crc on stream reconnect to avoid duplicate writes ([#16115](https://github.com/googleapis/google-cloud-cpp/pull/16115))
- fix(storage): Implement clean half-close stream teardown for appendable uploads ([#16112](https://github.com/googleapis/google-cloud-cpp/pull/16112))
- feat(storage): Add full object checksum validation for appendable uploads ([#16110](https://github.com/googleapis/google-cloud-cpp/pull/16110))
- feat(storage): Add read chunkwise checksum validation for sync grpc read ([#16107](https://github.com/googleapis/google-cloud-cpp/pull/16107))
- fix(storage): Resolve potential race condition in AsyncWriterConnectionImpl ([#16099](https://github.com/googleapis/google-cloud-cpp/pull/16099))
- feat(storage): Add delete object source field in ComposeObject API ([#16094](https://github.com/googleapis/google-cloud-cpp/pull/16094))
- feat(storage): Extend idempotency token use to all async mutation operations ([#16100](https://github.com/googleapis/google-cloud-cpp/pull/16100))

### [Common Libraries](/google/cloud/README.md)

- fix: remove deprecated ATOMIC_VAR_INIT usage ([#16127](https://github.com/googleapis/google-cloud-cpp/pull/16127))
- fix(testing_util): link opentelemetry-cpp SDK as PUBLIC ([#16101](https://github.com/googleapis/google-cloud-cpp/pull/16101))

### [Google APIs interface definitions](https://github.com/googleapis/googleapis)

- This release is based on definitions as of [2026-05-25T07:03:03-07:00](https://github.com/googleapis/googleapis/tree/ef19b7b7a73f19f33ab86c5b3603e9590025acd7)

## v3.5.0 - 2026-05

### [Bigtable](/google/cloud/bigtable/README.md)

- fix(bigtable): treat NOT_FOUND and PERMISSION_DENIED on channel refresh as success ([#16086](https://github.com/googleapis/google-cloud-cpp/pull/16086))
- feat(bigtable): add DeadlineOption ([#16085](https://github.com/googleapis/google-cloud-cpp/pull/16085))

### [Storage](/google/cloud/storage/README.md)

- fix(storage): Reset write offset on gRPC BidiWriteObject resumable uploads ([#16083](https://github.com/googleapis/google-cloud-cpp/pull/16083))
- fix(storage): Handle request transformation after gRPC BidiWriteObject redirects ([#16073](https://github.com/googleapis/google-cloud-cpp/pull/16073))

### [Google APIs interface definitions](https://github.com/googleapis/googleapis)

- This release is based on definitions as of [2026-04-22T13:12:27-07:00](https://github.com/googleapis/googleapis/tree/20ac242a6b3a723cb10c1a0201209261addaf7d8)

## v3.4.0 - 2026-04

### [Bigtable](/google/cloud/bigtable/README.md)

- Dynamic Channel Pool support has been added as an opt-in feature. It can be
  enabled via the [InstanceChannelAffinityOption](https://github.com/googleapis/google-cloud-cpp/blob/f3de489be4caaaf17e589c7fd7e427488c474f61/google/cloud/bigtable/options.h#L186)
  and configured via the [DynamicChannelPoolSizingPolicyOption](https://github.com/googleapis/google-cloud-cpp/blob/f3de489be4caaaf17e589c7fd7e427488c474f61/google/cloud/bigtable/options.h#L222).

### [Data Catalog](/google/cloud/datacatalog/README.md)

- Added Data Lineage Config Management library.([#16069](https://github.com/googleapis/google-cloud-cpp/pull/16069))

### [Dataplex](/google/cloud/dataplex/README.md)

- Added Business Glossary library.([#16072](https://github.com/googleapis/google-cloud-cpp/pull/16072))
- Added CMEK library.([#16072](https://github.com/googleapis/google-cloud-cpp/pull/16072))
- Added Data Products library.([#16072](https://github.com/googleapis/google-cloud-cpp/pull/16072))

### [Spanner](/google/cloud/spanner/README.md)

- feat(spanner): set read lock mode at client level ([#16068](https://github.com/googleapis/google-cloud-cpp/pull/16068))

### [Storage](/google/cloud/storage/README.md)

- feat(storage): add IsOpen API for zonal read operation ([#16063](https://github.com/googleapis/google-cloud-cpp/pull/16063))

### [Google APIs interface definitions](https://github.com/googleapis/googleapis)

- This release is based on definitions as of [2026-04-02T09:44:56-07:00](https://github.com/googleapis/googleapis/tree/c8ca5bce5cbabac76b8619bd8d63ac10bb0561a9)

## v3.3.0 - 2026-03

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Gemini Enterprise for Customer Experience API](/google/cloud/ces/README.md)
- [Cluster Director API](/google/cloud/hypercomputecluster/README.md)
- [Vector Search API](/google/cloud/vectorsearch/README.md)
- [Vision AI API](/google/cloud/visionai/README.md)
- [Workload Manager](/google/cloud/workloadmanager/README.md)

### [Storage](/google/cloud/storage/README.md)

- feat(storage): Get/Insert/Update/Patch Object contexts ([#15933](https://github.com/googleapis/google-cloud-cpp/pull/15933))

### [Google APIs interface definitions](https://github.com/googleapis/googleapis)

- This release is based on definitions as of [2026-02-23T01:04:53-08:00](https://github.com/googleapis/googleapis/tree/edfe7983b9d99d6244b4d7636d25fa6e752a2041)

## v3.2.0 - 2026-02

### [Cloud Key Management Service](/google/cloud/kms/README.md)

- feat: Added DeleteCryptoKey and DeleteCryptoKeyVersion RPCs to permanently remove resources.
- feat: Introduced the RetiredResource resource to track records of deleted keys and prevent the reuse of their resource names.
- feat: Added ListRetiredResources and GetRetiredResource RPCs to manage and view these records.

## v3.1.0 - 2026-02

### Default Retry Policy Changes

All libraries except Bigtable, PubSub, Spanner, and Storage have had their
default maximum retry policy duration reduced from 30 minutes to 10 minutes.

## v3.0.0 - 2026-02

**BREAKING CHANGES**

As previously announced, `google-cloud-cpp` now requires C++ >= 17. This is
motivated by similar changes in our dependencies, and because C++ 17 has been
the default C++ version in all the compilers we support for several years.

We think this change is large enough that deserves a major version bump to
signal the new requirements. Additionally, we have decommissioned previously
deprecated API features and have also made some previously optional dependencies
required.

**NOTE**: Please refer to the [V3 Migration Guide](/doc/v3-migration-guide.md) 
for details on updating existing applications using v1.x.y or v2.x.y.
