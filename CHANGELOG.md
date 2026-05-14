# Changelog

**NOTE**: [Future Breaking Changes](/doc/deprecated.md) lists anticipated
breaking changes in the upcoming 4.x release. This release is scheduled for
2027-12 or 2028-01.

**NOTE**: Please refer to the [V3 Migration Guide](/doc/v3-migration-guide.md) 
for details on updating existing applications using v1.x.y or v2.x.y.

## v3.6.0 - TBD

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
