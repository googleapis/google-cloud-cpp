# Changelog

**NOTE**: [Future Breaking Changes](/doc/deprecated.md) lists anticipated
breaking changes in the upcoming 3.x release. This release is scheduled for
2024-12 or 2025-01.

## v2.24.0 - TBD

### Updated Libraries

- [Cloud SQL Admin API](/google/cloud/sql/README.md) - several new clients:
  - `SqlAvailableDatabaseVersionsServiceClient`
  - `SqlEventsServiceClient`
  - `SqlIamPoliciesServiceClient`
  - `SqlInstanceNamesServiceClient`
  - `SqlRegionsServiceClient`
- [Content Warehouse](/google/cloud/contentwarehouse/README.md) - new
  `PipelineServiceClient`
- [Dialogflow CX API](/google/cloud/dialogflow_cx/README.md) - new
  `GeneratorsClient`
- [Discovery Engine](/google/cloud/discoveryengine/README.md) - several new
  clients:
  - `DataStoreServiceClient`
  - `EngineServiceClient`
  - `SiteSearchEngineServiceClient`
- [Network Connectivity API](/google/cloud/networkconnectivity/README.md) - new
  `PolicyBasedRoutingServiceClient`
- [Retail](/google/cloud/retail/README.md) - new `AnalyticsServiceClient`

## v2.23.0 - 2024-04

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [App Hub API](/google/cloud/apphub/README.md)
- [Backup and DR Service API](/google/cloud/backupdr/README.md)
- [Sovereign Controls by Partners](/google/cloud/cloudcontrolspartner/README.md)
- [Cloud Storage Control API](/google/cloud/storagecontrol/README.md) is now GA.

### Updated Libraries

- [AI Platform API](/google/cloud/aiplatform/README.md) - new
  `NotebookServiceClient` and `PersistentResourceServiceClient`
- [Cloud Dataplex API](/google/cloud/dataplex/README.md) - new
  `CatalogServiceClient`
- [Network Services API](/google/cloud/networkservices/README.md) - new
  `DepServiceClient`
- [SecurityCenter](/google/cloud/securitycenter/README.md) - add v2 version of the library

### [Bigtable](/google/cloud/bigtable/README.md)

- fix(bigtable): respect GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING ([#13748](https://github.com/googleapis/google-cloud-cpp/pull/13748))

### [Pub/Sub](/google/cloud/pubsub/README.md)

- docs(pubsub): add kinesis sample ([#13593](https://github.com/googleapis/google-cloud-cpp/pull/13593))

### [Spanner](/google/cloud/spanner/README.md)

- feat(spanner): add ExcludeTransactionFromChangeStreamsOption ([#13898](https://github.com/googleapis/google-cloud-cpp/pull/13898))
- feat(spanner): add spanner::Value support for TypeCode::FLOAT32 ([#13862](https://github.com/googleapis/google-cloud-cpp/pull/13862))
- feat(spanner): add samples for proto columns ([#13759](https://github.com/googleapis/google-cloud-cpp/pull/13759))
- feat(spanner): add integration tests for proto columns ([#13756](https://github.com/googleapis/google-cloud-cpp/pull/13756))
- feat(spanner): add spanner::Value support for ProtoEnum/ProtoMessage ([#13747](https://github.com/googleapis/google-cloud-cpp/pull/13747))
- feat(spanner): add representations for the Spanner PROTO/ENUM types ([#13743](https://github.com/googleapis/google-cloud-cpp/pull/13743))
- feat(spanner): add sample for instance-admin managed autoscaler ([#13740](https://github.com/googleapis/google-cloud-cpp/pull/13740))

### [Speech](/google/cloud/speech/README.md)

- fix(speech): enable location specific connections ([#13757](https://github.com/googleapis/google-cloud-cpp/pull/13757))

### [Storage](/google/cloud/storage/README.md)

- fix(storage): preserve full Status in default creds ([#13897](https://github.com/googleapis/google-cloud-cpp/pull/13897))
- fix(storage): respect `GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING` ([#13766](https://github.com/googleapis/google-cloud-cpp/pull/13766))
- feat(storage): support HNS-enabled buckets ([#13753](https://github.com/googleapis/google-cloud-cpp/pull/13753))
- feat(storage): support soft delete timestamps ([#13728](https://github.com/googleapis/google-cloud-cpp/pull/13728))

### [Common Libraries](/google/cloud/README.md)

- feat(common): introduce `FieldMaskOption` ([#13936](https://github.com/googleapis/google-cloud-cpp/pull/13936))
- docs(common): link to system parameters page ([#13937](https://github.com/googleapis/google-cloud-cpp/pull/13937))
- feat(common): `QuotaUserOption` for gRPC-based libs ([#13933](https://github.com/googleapis/google-cloud-cpp/pull/13933))
- fix(common): `clang-cl` needs a `const_cast<>` ([#13891](https://github.com/googleapis/google-cloud-cpp/pull/13891))
- fix(compute): shorten paths for Bazel+MSVC ([#13836](https://github.com/googleapis/google-cloud-cpp/pull/13836))
- feat(common): support injecting custom headers ([#13829](https://github.com/googleapis/google-cloud-cpp/pull/13829))

## v2.22.0 - 2024-03

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Security Center Management API](/google/cloud/securitycentermanagement/README.md)

### [Bigtable](/google/cloud/bigtable/README.md)

- feat: promote `EnableServerRetriesOption` ([#13698](https://github.com/googleapis/google-cloud-cpp/pull/13698))
- feat(bigtable): install mocks library ([#13551](https://github.com/googleapis/google-cloud-cpp/pull/13551))

### [Pub/Sub](/google/cloud/pubsub/README.md)

- docs(pubsub): add doxygen comment to deprecate handwritten apis ([#13585](https://github.com/googleapis/google-cloud-cpp/pull/13585))

### [Spanner](/google/cloud/spanner/README.md)

- feat(spanner): add support for max commit delay ([#13562](https://github.com/googleapis/google-cloud-cpp/pull/13562))
- feat: install mocks for `spanner`, `sql`, `pubsublite` ([#13548](https://github.com/googleapis/google-cloud-cpp/pull/13548))

### [Storage](/google/cloud/storage/README.md)

- docs(GCS+gRPC): add contact info ([#13688](https://github.com/googleapis/google-cloud-cpp/pull/13688))
- feat(storage): support listing folders as prefixes ([#13683](https://github.com/googleapis/google-cloud-cpp/pull/13683))
- feat(storage): support soft-deleted objects ([#13644](https://github.com/googleapis/google-cloud-cpp/pull/13644))
- feat(storage): support Bucket soft-delete metadata ([#13623](https://github.com/googleapis/google-cloud-cpp/pull/13623))

### [Common Libraries](/google/cloud/README.md)

- feat: cmake option to skip building mock libraries ([#13673](https://github.com/googleapis/google-cloud-cpp/pull/13673))
- feat(generator): support `request_id`-like fields ([#13615](https://github.com/googleapis/google-cloud-cpp/pull/13615))
- docs(common): advertise OAuth2 library ([#13614](https://github.com/googleapis/google-cloud-cpp/pull/13614))

## v2.21.0 - 2024-02

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Personalized Service Health](/google/cloud/servicehealth/README.md)

### [Bigtable](/google/cloud/bigtable/README.md)

- feat(bigtable): support bigtable routing cookies ([#13502](https://github.com/googleapis/google-cloud-cpp/pull/13502))
- feat(bigtable): logging for bulk apply throttling ([#13488](https://github.com/googleapis/google-cloud-cpp/pull/13488))

### [Pub/Sub](/google/cloud/pubsub/README.md)

- fix(pubsub): install admin mocks ([#13537](https://github.com/googleapis/google-cloud-cpp/pull/13537))
- feat(pubsub): add lease management for unary pull ([#13428](https://github.com/googleapis/google-cloud-cpp/pull/13428))

### [Storage](/google/cloud/storage/README.md)

- feat(storage): support `UniverseDomainOption` ([#13471](https://github.com/googleapis/google-cloud-cpp/pull/13471))
- docs(GCS+gRPC): better comments for `AsyncConnection` ([#13424](https://github.com/googleapis/google-cloud-cpp/pull/13424))

### [Common Libraries](/google/cloud/README.md)

- feat: support UniverseDomainOption in gRPC IAM stub ([#13466](https://github.com/googleapis/google-cloud-cpp/pull/13466))
- feat: simplify Bazel initialization ([#13411](https://github.com/googleapis/google-cloud-cpp/pull/13411))

## v2.20.0 - 2024-01

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Cloud Quotas](/google/cloud/cloudquotas/README.md)

### [AI Platform/Vertex AI](/google/cloud/aiplatform/README.md)

- feat(aiplatform): add new service proto file ([#13325](https://github.com/googleapis/google-cloud-cpp/pull/13325))

### [Bigtable](/google/cloud/bigtable/README.md)

- fix(bigtable): use FQDN ([#13305](https://github.com/googleapis/google-cloud-cpp/pull/13305))

### [OpenTelemetry](/google/cloud/opentelemetry/README.md)

- feat(otel): capture gRPC response metadata in traces ([#13278](https://github.com/googleapis/google-cloud-cpp/pull/13278))

### [Pub/Sub](/google/cloud/pubsub/README.md)

- fix(pubsub): get future value before returning ([#13241](https://github.com/googleapis/google-cloud-cpp/pull/13241))
- fix(pubsub): pass by reference explicitly ([#13220](https://github.com/googleapis/google-cloud-cpp/pull/13220))

### [Common Libraries](/google/cloud/README.md)

- fix(common): returnable argument to `.then()` ([#13316](https://github.com/googleapis/google-cloud-cpp/pull/13316))
- feat(common): read-write streaming RPCs metadata ([#13228](https://github.com/googleapis/google-cloud-cpp/pull/13228))
- fix: respect logging format env var in `compute`, `sql` ([#13368](https://github.com/googleapis/google-cloud-cpp/pull/13368))

## v2.19.0 - 2023-12

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Distributed Cloud Edge Network API](/google/cloud/edgenetwork/README.md)
- [Cloud SQL Admin API](/google/cloud/sql/README.md)
- [Telecom Network Automation API](google/cloud/telcoautomation/README.md)

### [Bigtable](/google/cloud/bigtable/README.md)

- feat(bigtable): throttling for AsyncBulkApply ([#13203](https://github.com/googleapis/google-cloud-cpp/pull/13203))
- fix(bigtable): async context propagation for tracing ([#13156](https://github.com/googleapis/google-cloud-cpp/pull/13156))
- feat(bigtable): support BulkApply throttling ([#13124](https://github.com/googleapis/google-cloud-cpp/pull/13124))

### [OpenTelemetry](/google/cloud/opentelemetry/README.md)

- feat(otel): add Makefile ([#13177](https://github.com/googleapis/google-cloud-cpp/pull/13177))
- fix(otel): detach context when single-threaded ([#13154](https://github.com/googleapis/google-cloud-cpp/pull/13154))
- fix(otel): reconnect async traces (e.g. LROs) ([#13147](https://github.com/googleapis/google-cloud-cpp/pull/13147))

### [Spanner](/google/cloud/spanner/README.md)

- feat: add PG.OID type ([#13127](https://github.com/googleapis/google-cloud-cpp/pull/13127))
- feat(spanner): control replicas/regions used in non-transactional reads ([#13031](https://github.com/googleapis/google-cloud-cpp/pull/13031))

### [Vertex AI](/google/cloud/aiplatform/README.md)

- feat(aiplatform): generate new services ([#13150](https://github.com/googleapis/google-cloud-cpp/pull/13150))

### [Common Libraries](/google/cloud/README.md)

- fix(common): use FQDN for the metadata service ([#13139](https://github.com/googleapis/google-cloud-cpp/pull/13139))
- feat(common): add GrpcCompressionAlgorithmOption ([#13108](https://github.com/googleapis/google-cloud-cpp/pull/13108))
- doc: fix cliffhanger ([#13033](https://github.com/googleapis/google-cloud-cpp/pull/13033))

## v2.18.0 - 2023-11

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Secure Source Manager](/google/cloud/securesourcemanager/README.md)

### [Compute Engine](/google/cloud/compute/README.md)

- fix(compute): `RegionSecurityPoliciesClient::PatchSecurityPolicy` added
  `update_mask` field.

### [Memorystore for Redis](/google/cloud/redis/README.md)

New `CloudRedisClusterClient`.

### [OpenTelemetry](/google/cloud/opentelemetry/README.md)

- doc(otel): add packaging instructions
  ([#13005](https://github.com/googleapis/google-cloud-cpp/pull/13005))
- fix(otel): end async client spans properly
  ([#12919](https://github.com/googleapis/google-cloud-cpp/pull/12919))
- fix(otel): support abseil \<= 20210324
  ([#12993](https://github.com/googleapis/google-cloud-cpp/pull/12993))
-

### [Spanner](/google/cloud/spanner/README.md)

- feat(spanner): single-RPC, batched commit of mutation groups
  ([#12930](https://github.com/googleapis/google-cloud-cpp/pull/12930))

### [Common Libraries](/google/cloud/README.md)

- feat(common): mock for `AsyncStreamingReadWriteRpc`
  ([#12950](https://github.com/googleapis/google-cloud-cpp/pull/12950))
- fix(common): missed using `CARootsFilePathOption`
  ([#12997](https://github.com/googleapis/google-cloud-cpp/pull/12997))
- fix(generator): do not override default sleeper in streaming-read RPCs
  ([#12920](https://github.com/googleapis/google-cloud-cpp/pull/12920))

### Other Changes

- fix(oauth2): only enable via `GOOGLE_CLOUD_CPP_ENABLE`
  ([#12911](https://github.com/googleapis/google-cloud-cpp/pull/12911)) When
  compiling with CMake, the `oauth2` used to be automatically enabled if
  `GOOGLE_CLOUD_CPP_REST` was manually enabled or enabled by a separate library.
  That made it impossible to shard a build with separate builds for `oauth2`,
  `storage` and `compute`.

## v2.17.0 - 2023-10

### [Compute Engine](/google/cloud/compute/README.md)

- fix(compute): build with Bazel and MSVC
  ([#12877](https://github.com/googleapis/google-cloud-cpp/pull/12877))
- fix(compute): usable in external Bazel projects
  ([#12864](https://github.com/googleapis/google-cloud-cpp/pull/12864))
- fix(compute): add missing bazelrc file
  ([#12856](https://github.com/googleapis/google-cloud-cpp/pull/12856))

### [Storage](/google/cloud/storage/README.md)

- feat(storage): another Bucket CRUD field
  ([#12842](https://github.com/googleapis/google-cloud-cpp/pull/12842))
- fix(storage): fix tellg() values
  ([#12844](https://github.com/googleapis/google-cloud-cpp/pull/12844))
- fix(storage): check ReadObject options at runtime
  ([#12841](https://github.com/googleapis/google-cloud-cpp/pull/12841))
- feat(storage): `MatchGlob` for `ListObjects()`
  ([#12840](https://github.com/googleapis/google-cloud-cpp/pull/12840))
- doc(storage): describe default project search
  ([#12833](https://github.com/googleapis/google-cloud-cpp/pull/12833))

### [Common Libraries](/google/cloud/README.md)

- fix(oauth2): do not require `id_token` in creds
  ([#12867](https://github.com/googleapis/google-cloud-cpp/pull/12867))

### Other Changes

**We have stopped testing with GCC 7.5.0 on openSUSE/Leap:** this distro uses
GCC 7.5.0 by default. This specific version of GCC crashes when compiling some
of the files generated by Protobuf 24.x. In other distros we test with GCC 7.3,
without problems. And we also test with newer versions of GCC without problems.
The exact files that cause the compiler to crash seem to change with minor
changes in Protobuf or the input `.proto` files, making it difficult to maintain
some kind of exclusion list.

We have stopped testing with GCC 7.5.0 on openSUSE/Leap, and recommend you use
GCC >= 8 to compile `google-cloud-cpp`. If you must compile with GCC 7.5.0,
consider only enabling the libraries you will need.

## v2.16.0 - 2023-10

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Compute Engine](/google/cloud/compute/README.md)
- [Infrastructure Manager](/google/cloud/config/README.md)
- [NetApp](/google/cloud/netapp/README.md)

### [Discovery Engine](/google/cloud/discoveryengine/README.md)

New `*Client` for conversational search.

### [OAuth2](/google/cloud/oauth2/README.md)

- feat(oauth2): add quickstart, README, etc.
  ([#12754](https://github.com/googleapis/google-cloud-cpp/pull/12754))

### [OpenTelemetry](/google/cloud/opentelemetry/README.md)

We instrumented the Google Cloud C++ libraries for [distributed tracing] using
[OpenTelemetry]. All client libraries are instrumented in some capacity.

Features to enable the collection and export of these traces are now GA. See the
[quickstart][otel-quickstart] within the library to learn more about these
tools.

### [Policy Troubleshooter](/google/cloud/policytroubleshooter/README.md)

Remove `PoliciesClient`. This client was placed in the wrong library. The
correct location is google/cloud/iam/v2.

### [Storage](/google/cloud/storage/README.md)

- fix(storage): trace auth when default credentials are assumed
  ([#12672](https://github.com/googleapis/google-cloud-cpp/pull/12672))
- docs(storage): more on InsertObject vs. WriteObject
  ([#12577](https://github.com/googleapis/google-cloud-cpp/pull/12577))

### [Common Libraries](/google/cloud/README.md)

- feat(common): new `*Option` to configure HTTP proxy
  ([#12766](https://github.com/googleapis/google-cloud-cpp/pull/12766))
- fix: export headers with top-level Bazel targets
  ([#12762](https://github.com/googleapis/google-cloud-cpp/pull/12762))
- fix: gRPC auth logging enabled by "auth"
  ([#12702](https://github.com/googleapis/google-cloud-cpp/pull/12702))
- fix: add implicit routing in GAPICs
  ([#12544](https://github.com/googleapis/google-cloud-cpp/pull/12544))

### Known issues

Compiling gRPC with Bazel and Clang >= 16 requires `--features=-layering_check`
in your Bazel command-line. For more details, see [grpc#34482].

## v2.15.0 - 2023-09

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Datastore](/google/cloud/datastore/README.md)
- [Discovery Engine API](/google/cloud/discoveryengine/README.md)
- [Policy Simulator](/google/cloud/policysimulator/README.md)
- [Policy Troubleshooter](/google/cloud/policytroubleshooter/README.md)

### Cloud IoT

The Cloud IoT Core service has been [shutdown][cloud-iot-shutdown]. We removed
the corresponding C++ client library as it is no longer usable.

### [BeyondCorp API](google/cloud/beyondcorp/README.md)

Parts of the BeyondCorp API are restricted to only existing customers and will
be decommissioned. No C++ customers are affected, so we are removing the
unusable components from the C++ libraries. More information in the BeyondCorp
[announcement](https://cloud.google.com/beyondcorp-enterprise/docs/release-notes#March_31_2023).

### [Natural Language AI](/google/cloud/language/README.md)

We expanded the library to include the `v2` version of the API.

### [Notebooks](/google/cloud/notebooks/README.md)

We expanded the library to include the `v2` version of the API.

### [Pub/Sub](/google/cloud/pubsub/README.md)

- fix(pubsub): url encode routing params on publish
  ([#12454](https://github.com/googleapis/google-cloud-cpp/pull/12454))
- feat(pubsub): increase backoff from 1.3 to 4.0
  ([#12306](https://github.com/googleapis/google-cloud-cpp/pull/12306))

### [Spanner](/google/cloud/spanner/README.md)

- feat(spanner): enable leader aware routing by default
  ([#12319](https://github.com/googleapis/google-cloud-cpp/pull/12319))

### [Storage](/google/cloud/storage/README.md)

- fix(storage): tracing should decorate retries
  ([#12314](https://github.com/googleapis/google-cloud-cpp/pull/12314))

### [Common Libraries](/google/cloud/README.md)

- fix: URL encode explicit routing values
  ([#12493](https://github.com/googleapis/google-cloud-cpp/pull/12493))
  ([#12447](https://github.com/googleapis/google-cloud-cpp/pull/12447))
- feat(common): add `Options::set<>() &&` overload
  ([#12424](https://github.com/googleapis/google-cloud-cpp/pull/12424))
- fix(common): gRPC alarms require more locking
  ([#12406](https://github.com/googleapis/google-cloud-cpp/pull/12406))
- feat(common): retry loops preserve original message
  ([#12368](https://github.com/googleapis/google-cloud-cpp/pull/12368))
- feat(otel): control tracing with environment variable
  ([#11897](https://github.com/googleapis/google-cloud-cpp/pull/11897))
- docs(otel): troubleshoot exporter doc
  ([#12345](https://github.com/googleapis/google-cloud-cpp/pull/12345))
- docs(otel): build quickstart with vcpkg
  ([#12324](https://github.com/googleapis/google-cloud-cpp/pull/12324))
- feat(common): retry loops signal via error info
  ([#12313](https://github.com/googleapis/google-cloud-cpp/pull/12313))
- fix(common): handle expired policies in `*RetryLoop`
  ([#12301](https://github.com/googleapis/google-cloud-cpp/pull/12301))

### Other Changes

**Bazel Testing**: from v2.15.0 we will only test with Bazel >= 6.0.0. We do not
consider this a breaking change, as Bazel 5.x has been in maintenance mode for
more than 6 months.

**CMake Proto Libraries**: We only compile service-specific protos if the
corresponding client library is enabled, via `-DGOOGLE_CLOUD_CPP_ENABLE=...`.

This change reduces build times for customers who use CMake but who are not
using **all** of the client libraries listed below.

We considered it [a bug][#8022] that customers building with CMake were forced
to compile large proto libraries that they did not need. It was certainly
[confusing][#10174].

Any change in behavior, including fixing bugs, can be considered "breaking". By
policy we don't consider bug fixes to be breaking changes. We applied that
policy in this case.

The impacted libraries are:

- `bigquery`
- `bigtable`
- `dialogflow_es`
- `iam`
- `logging`
- `pubsub`
- `speech`
- `storage`
- `texttospeech`
- `trace`

If you are dependent on any of these proto libraries **and** you are not
compiling the corresponding client library, you will need to update your build
scripts.

For example, if you depend on `google_cloud_cpp_speech_protos` (e.g., if you
have been using this library to make calls to Cloud Speech using raw gRPC), add
`-DGOOGLE_CLOUD_CPP_ENABLE=...,speech,...` to your CMake configure command. If
you build with `vcpkg`, include `speech` in your install command.

Note that `google_cloud_cpp_storage_protos` are associated with the
`experimental-storage_grpc` feature, not the `storage` feature.

## v2.14.0 - 2023-08

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Commerce](/google/cloud/commerce/README.md)

### [Bigtable](/google/cloud/bigtable/README.md)

- docs(bigtable): document `DataRetryPolicy`
  ([#12040](https://github.com/googleapis/google-cloud-cpp/pull/12040))
- feat(bigtable): reverse scans
  ([#12022](https://github.com/googleapis/google-cloud-cpp/pull/12022))

### [Channel Services](/google/cloud/channel/README.md)

The library has been expanded to include the Reporting API.

### [Cloud Build](/google/cloud/cloudbuild/README.md)

- feat(cloudbuild): promote 2nd gen (repositories) API to GA
  ([#12236](https://github.com/googleapis/google-cloud-cpp/pull/12236))

### [Cloud Functions](/google/cloud/functions/README.md)

The library has been expanded to include the [2nd gen][functions-v2] API.

### [Cloud Monitoring](/google/cloud/monitoring/README.md)

The library has been expanded to include the [Snooze][monitoring-snooze] API.

### [Cloud Run](/google/cloud/run/README.md)

The library has been expanded to include the [Job Executions][cloud-run-jobs]
API.

### [Compute Engine OS Config](/google/cloud/osconfig/README.md)

The library has been expanded to include the Zonal OS Config API.

### [Data Catalog](/google/cloud/datcatalog/README.md)

- feat(datacatalog): promote `datalineage` to GA
  ([#12271](https://github.com/googleapis/google-cloud-cpp/pull/12271))

### [Dataproc](/google/cloud/dataproc/README.md)

The library has been expanded to include the [Node Groups][dataproc-node-groups]
API.

### [Logging](/google/cloud/logging/README.md)

The library has been expanded to include the
[Log-based Metrics][logging-metrics] API and the [Log Router][logging-config]
API.

### [Pub/Sub](/google/cloud/pubsub/README.md)

- docs(pubsub): add cloud storage subscription sample
  ([#12088](https://github.com/googleapis/google-cloud-cpp/pull/12088))
- docs(pubsub): add unwrapped subscription sample
  ([#12090](https://github.com/googleapis/google-cloud-cpp/pull/12090))
- docs(pubsub): add a subscriber quickstart
  ([#12053](https://github.com/googleapis/google-cloud-cpp/pull/12053))
- docs(pubsub): document `RetryPolicy` interface
  ([#12030](https://github.com/googleapis/google-cloud-cpp/pull/12030))

### [Retail](/google/cloud/retail/README.md)

The library has been expanded to include new services.

### [Resource Manager](/google/cloud/resourcemanager/README.md)

The library has been expanded to include the [Tags][resource-manager-tags] API.

### [Spanner](/google/cloud/spanner/README.md)

- feat(spanner): samples for bit-reversed sequence
  ([#12280](https://github.com/googleapis/google-cloud-cpp/pull/12280))
- doc(spanner): mark obsolete Spanner options classes as deprecated
  ([#12256](https://github.com/googleapis/google-cloud-cpp/pull/12256))
- doc(spanner): Improve documentation for ActionOnExhaustion
  ([#12238](https://github.com/googleapis/google-cloud-cpp/pull/12238))
- doc(spanner): add documentation/sample for TransactionRerunPolicy
  ([#12140](https://github.com/googleapis/google-cloud-cpp/pull/12140))
- feat(spanner): tests and samples for foreign-key delete cascade
  ([#12122](https://github.com/googleapis/google-cloud-cpp/pull/12122))
- docs(spanner): document `RetryPolicy` interface
  ([#12037](https://github.com/googleapis/google-cloud-cpp/pull/12037))

### [Speech](/google/cloud/speech/README.md)

The library has been expanded to include the
[Model Adaptation][speech-model-adaptation] API.

### [Storage](/google/cloud/storage/README.md)

- docs(storage): document `RetryPolicy` interface
  ([#12031](https://github.com/googleapis/google-cloud-cpp/pull/12031))

### [Common Libraries](/google/cloud/README.md)

- fix(common): add missing library on Windows
  ([#12103](https://github.com/googleapis/google-cloud-cpp/pull/12103))
- fix(common): avoid run-time conflicts on Windows
  ([#12105](https://github.com/googleapis/google-cloud-cpp/pull/12105))
- feat(oauth2): new library to create oauth2 tokens
  ([#12064](https://github.com/googleapis/google-cloud-cpp/pull/12064))
- feat: support split packages
  ([#12049](https://github.com/googleapis/google-cloud-cpp/pull/12049))

## v2.13.0 - 2023-07

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Cloud Data Fusion](/google/cloud/datafusion/README.md)
- [Dataproc Metastore](/google/cloud/metastore/README.md)
- [Migration Center](google/cloud/migrationcenter/README.md)
- [Rapid Migration Assessment](/google/cloud/rapidmigrationassessment/README.md)
- [Secure Web Proxy](/google/cloud/networksecurity/README.md)

### [Batch](/google/cloud/batch/README.md)

- docs: an example to extract Cloud Batch logs
  ([#11935](https://github.com/googleapis/google-cloud-cpp/pull/11935))
- docs(batch): introduce basic snippets
  ([#11915](https://github.com/googleapis/google-cloud-cpp/pull/11915))

### [Spanner](/google/cloud/spanner/README.md)

- doc(spanner): add an example for CommitAtLeastOnce()
  ([#11905](https://github.com/googleapis/google-cloud-cpp/pull/11905))
- feat(spanner): implement at-least-once Commit
  ([#11899](https://github.com/googleapis/google-cloud-cpp/pull/11899))

### [Common Libraries](/google/cloud/README.md)

- feat(generator): separate page for retry policy overrides
  ([#11950](https://github.com/googleapis/google-cloud-cpp/pull/11950))
- fix(common): pagination must support empty pages
  ([#11937](https://github.com/googleapis/google-cloud-cpp/pull/11937))
- feat(generator): create retry policy samples
  ([#11930](https://github.com/googleapis/google-cloud-cpp/pull/11930))
- fix(common): use 64-bit API on Windows to get file sizes
  ([#11906](https://github.com/googleapis/google-cloud-cpp/pull/11906))

## v2.12.0 - 2023-06

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Content Warehouse](/google/cloud/contentwarehouse/README.md)
- [Cloud Domains](/google/cloud/domains/README.md)
- [Essential Contacts](/google/cloud/essentialcontacts/README.md)
- [reCAPTCHA Enterprise](/google/cloud/recaptchaenterprise/README.md)
- [Timeseries Insights](/google/cloud/timeseriesinsights/README.md)
- [Traffic Director](/google/cloud/networkservices/README.md)
- [Vertex AI](/google/cloud/aiplatform/README.md)

### Cloud Debugger

The Cloud Debugger service (aka Stackdriver Debugger API) has been
[shutdown][cloud-debugger-deprecated]. The corresponding client library has been
removed.

### [BigQuery](/google/cloud/bigquery/README.md)

This library has been expanded to include the BigLake API
([#11882](https://github.com/googleapis/google-cloud-cpp/pull/11882))

### [Pub/Sub](/google/cloud/pubsub/README.md)

- doc(pubsub): add new samples for schemas
  ([#11872](https://github.com/googleapis/google-cloud-cpp/pull/11872))
  ([#11848](https://github.com/googleapis/google-cloud-cpp/pull/11848))
  ([#11840](https://github.com/googleapis/google-cloud-cpp/pull/11840))

### [Common Libraries](/google/cloud/README.md)

- docs: use c.g.com/cpp mocking guide
  ([#11869](https://github.com/googleapis/google-cloud-cpp/pull/11869))
- docs: link reference docs at c.g.c/cpp/docs/reference
  ([#11799](https://github.com/googleapis/google-cloud-cpp/pull/11799))
- fix(generator): correct URL for reference docs
  ([#11765](https://github.com/googleapis/google-cloud-cpp/pull/11765))

## v2.11.0 - 2023-06

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Cloud Support API](/google/cloud/support/README.md)

### [Bigtable](/google/cloud/bigtable/README.md)

- docs(bigtable): async Table APIs are stable
  ([#11711](https://github.com/googleapis/google-cloud-cpp/pull/11711))
- doc(bigtable): deprecate DataClient in doxygen
  ([#11550](https://github.com/googleapis/google-cloud-cpp/pull/11550))

### [Cloud Asset](/google/cloud/asset/README.md)

The library has been re-enabled on Windows. See
[#11714](https://github.com/googleapis/google-cloud-cpp/issues/11714) for
details.

### [Spanner](/google/cloud/spanner/README.md)

- docs(spanner): some documentation tweaks
  ([#11641](https://github.com/googleapis/google-cloud-cpp/pull/11641))
- feat(spanner): tests and samples for drop-database protection
  ([#11637](https://github.com/googleapis/google-cloud-cpp/pull/11637))
- feat(spanner): make `ResultSourceInterface` public
  ([#11636](https://github.com/googleapis/google-cloud-cpp/pull/11636))

### [Storage](/google/cloud/storage/README.md)

- fix(storage): consistent project id overrides
  ([#11754](https://github.com/googleapis/google-cloud-cpp/pull/11754))
- feat(storage): better error messages in signed URLs
  ([#11741](https://github.com/googleapis/google-cloud-cpp/pull/11741))
- feat(storage): offer mock client without decorators
  ([#11697](https://github.com/googleapis/google-cloud-cpp/pull/11697))
- feat(storage): lazy allocation for upload buffer
  ([#11633](https://github.com/googleapis/google-cloud-cpp/pull/11633))

### [Storage Transfer Service](/google/cloud/storagetransfer/README.md)

The library has been re-enabled on macOS. See
[#8785](https://github.com/googleapis/google-cloud-cpp/issues/8785) for details.

### [Common Libraries](/google/cloud/README.md)

- feat: use full jitter exp backoff policy in the generator
  ([#11748](https://github.com/googleapis/google-cloud-cpp/pull/11748))
- feat: add new constructor for exponential backoff policy
  ([#11650](https://github.com/googleapis/google-cloud-cpp/pull/11650))
- feat: avoid development dependencies with Bazel
  ([#11724](https://github.com/googleapis/google-cloud-cpp/pull/11724))
- docs(common): better exception descriptions
  ([#11705](https://github.com/googleapis/google-cloud-cpp/pull/11705))
- fix(rest): support rewinds in libcurl
  ([#11703](https://github.com/googleapis/google-cloud-cpp/pull/11703),
  [#11709](https://github.com/googleapis/google-cloud-cpp/pull/11709))
- fix: workaround curl_multi_poll returning an error on EINTR
  ([#11649](https://github.com/googleapis/google-cloud-cpp/pull/11649))
- docs(common): improve `StatusOr<>` documentation
  ([#11631](https://github.com/googleapis/google-cloud-cpp/pull/11631))
- fix: Correct exponential backoff ranges
  ([#11529](https://github.com/googleapis/google-cloud-cpp/pull/11529))
- fix: patch releases do not change the ABI
  ([#11499](https://github.com/googleapis/google-cloud-cpp/pull/11499))
- fix(rest): missing user-agent separator
  ([#11473](https://github.com/googleapis/google-cloud-cpp/pull/11473))

## v2.10.0 - 2023-05

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [ConfidentialComputing API](/google/cloud/confidentialcomputing/README.md)
- [Storage Insights API](/google/cloud/storageinsights/README.md)
- [Workstations API](/google/cloud/workstations/README.md)

The following experimental libraries are now available:

- [Cloud SQL Admin API](/google/cloud/sql/README.md)

### [Bigquery](/google/cloud/bigquery/README.md)

- Removed bigquery/v2/model\*. There are no plans to implement gRPC endpoints
  for this service. Therefore the generated code will never be usable and never
  has been.

### [Service Control](/google/cloud/servicecontrol/README.md)

The library has been expanded to include the v2 service.

### [Spanner](/google/cloud/spanner/README.md)

- fix(spanner): propagate Options through SessionPool callbacks
  ([#11344](https://github.com/googleapis/google-cloud-cpp/pull/11344))

### [Storage](/google/cloud/storage/README.md)

- fix(storage): cache legacy credentials
  ([#11271](https://github.com/googleapis/google-cloud-cpp/pull/11271))
- fix(storage): support per-request options
  ([#11445](https://github.com/googleapis/google-cloud-cpp/pull/11445))
- fix(storage): cache all credential types
  ([#11447](https://github.com/googleapis/google-cloud-cpp/pull/11447))

### [Common Libraries](/google/cloud/README.md)

- doc: declutter main.dox pages
  ([#11405](https://github.com/googleapis/google-cloud-cpp/pull/11405))
- doc(common): documentation improvements
  ([#11376](https://github.com/googleapis/google-cloud-cpp/pull/11376))

### Other Changes

**Bazel Testing**: after v2.10.0 we will only test with Bazel >= 5.4.0. We do
not consider this a breaking change, as Bazel 4.x has been in maintenance mode
for several months.

## v2.9.0 - 2023-04

### [Cloud Build](/google/cloud/cloudbuild/README.md)

The library has been expanded to include the Cloud Build repositories (2nd gen)
API. Note that the client is tagged as experimental, because the service is
still in [preview][product-launch-stages].

### [Cloud Trace](/google/cloud/trace/README.md)

The library has been expanded to include the v1 service.

### [IAM](/google/cloud/iam/README.md)

- fix(iam): override idempotency on idempotent POST methods
  ([#11045](https://github.com/googleapis/google-cloud-cpp/pull/11045))

### [Spanner](/google/cloud/spanner/README.md)

- feat(spanner): add the final pieces for the RouteToLeaderOption
  ([#11112](https://github.com/googleapis/google-cloud-cpp/pull/11112))
- feat(spanner): support "data boost" on partitioned queries and reads
  ([#10998](https://github.com/googleapis/google-cloud-cpp/pull/10998))

### [Storage](/google/cloud/storage/README.md)

- feat(storage): reduce copies in `InsertObject()`
  ([#11014](https://github.com/googleapis/google-cloud-cpp/pull/11014))

### [Common Libraries](/google/cloud/README.md)

- feat: consume less entropy for PRNG
  ([#11102](https://github.com/googleapis/google-cloud-cpp/pull/11102))
- feat(mocks): provide access to call options in client tests
  ([#11050](https://github.com/googleapis/google-cloud-cpp/pull/11050))

### Testing

We have stopped testing with MSVC 2017. Microsoft stopped mainstream support for
MSVC 2017 in
[2022-04](https://learn.microsoft.com/en-us/lifecycle/products/visual-studio-2017).
We continue to test with MSVC 2022 and MSVC 2019. We recommend that you update
to one of these versions to use more recent versions of the `google-cloud-cpp`
libraries. Note that, in accordance with Google's
[Foundational C++ support policy][oss-cxx-support], the other Google libraries
have stopped (or shortly will stop) testing with MSVC 2017.

## v2.8.0 - 2023-03

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Advisory Notifications](/google/cloud/advisorynotifications/README.md)
- [Alloy DB](/google/cloud/alloydb/README.md)
- [API Keys](/google/cloud/apikeys/README.md)

### [Bigtable](/google/cloud/bigtable/README.md)

- fix(bigtable): retries for CheckConsistency / AsyncWaitForConsistency
  ([#10955](https://github.com/googleapis/google-cloud-cpp/pull/10955))
- docs(bigtable): clean up CreateTable sample
  ([#10844](https://github.com/googleapis/google-cloud-cpp/pull/10844))

### [Data Catalog](/google/cloud/datacatalog/README.md)

- feat(datacatalog): generate lineage library
  ([#10977](https://github.com/googleapis/google-cloud-cpp/pull/10977))

### [KMS](/google/cloud/kms/README.md)

The library has been expanded to include the KMS Inventory API.

### [Pub/Sub](/google/cloud/pubsub/README.md)

- fix(pubsub): no warnings on `ack()/nack()` success
  ([#10920](https://github.com/googleapis/google-cloud-cpp/pull/10920))
- fix(pubsub): fewer default threads for 32-bit builds
  ([#10793](https://github.com/googleapis/google-cloud-cpp/pull/10793))

### [TPU](/google/cloud/tpu/README.md)

The library has been expanded to include the TPU v2 API.

### [Common Libraries](/google/cloud/README.md)

We have introduced versioned clients for many services. The version is that of
the GCP service. While this naming convention is more verbose, it allows us to
support clients for multiple versions of a GCP service from within the same
library (e.g. `speech_v1::SpeechClient` and `speech_v2::SpeechClient`). See
[#10170] for more details.

- fix: retries for GetIamPolicy, TestIamPermissions
  ([#10957](https://github.com/googleapis/google-cloud-cpp/pull/10957))
- doc: improve description for "terminate" group
  ([#10950](https://github.com/googleapis/google-cloud-cpp/pull/10950))
- fix(common): fewer spurious warnings in the log
  ([#10811](https://github.com/googleapis/google-cloud-cpp/pull/10811))

## v2.7.0 - 2023-02

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Anthos Multi-Cloud API](/google/cloud/gkemulticloud/README.md)

### [Pub/Sub](/google/cloud/pubsub/README.md)

- Replaced the wrappers for [google.pubsub.v1.SchemaServiceClient] with
  automatically generated code. Our telemetry indicates that there are no C++
  applications using this code, and therefore we do not consider this a breaking
  change.

### [Common Libraries](/google/cloud/README.md)

- fix reference links in documentation
  ([#10687](https://github.com/googleapis/google-cloud-cpp/pull/10687),
  [#10684](https://github.com/googleapis/google-cloud-cpp/pull/10687))
- fix: interface proto libraries work with older CMake
  ([#10636](https://github.com/googleapis/google-cloud-cpp/pull/10636))
- fix(common): missing Abseil deps in pkgconfig
  ([#10616](https://github.com/googleapis/google-cloud-cpp/pull/10616))
- doc: declutter generated README files
  ([#10562](https://github.com/googleapis/google-cloud-cpp/pull/10562))
- doc(common): in-depth guide for `StatusOr`
  ([#10555](https://github.com/googleapis/google-cloud-cpp/pull/10555))
- doc: remove boilerplate from landing page snippet
  ([#10537](https://github.com/googleapis/google-cloud-cpp/pull/10537))
- doc(common): use real Doxygen groups
  ([#10504](https://github.com/googleapis/google-cloud-cpp/pull/10504))
- fix(generator): correct doxygen comments
  ([#10500](https://github.com/googleapis/google-cloud-cpp/pull/10500))

## v2.6.0 - 2023-01

### [BigQuery](/google/cloud/bigquery/README.md)

The library has been expanded to include the following services:

- [BigQuery Data Policy API](https://cloud.google.com/bigquery/docs/column-data-masking-intro)

### [IAM](/google/cloud/iam/README.md)

The library has been expanded to include the IAM v2 API. This API includes
support for [IAM Deny](https://cloud.google.com/iam/docs/deny-overview)
policies.

### [Pub/Sub](/google/cloud/pubsub/README.md)

- fix: add missing <cstdint> includes
  ([#10421](https://github.com/googleapis/google-cloud-cpp/pull/10421))

### [Speech](/google/cloud/speech/README.md)

- fix: remove duplicate protos
  ([#10486](https://github.com/googleapis/google-cloud-cpp/pull/10486))

### [Storage](/google/cloud/storage/README.md)

- fix(storage): better error code for CreateBucket() and 409 errors
  ([#10480](https://github.com/googleapis/google-cloud-cpp/pull/10480)
- fix: add missing <cstdint> includes
  ([#10421](https://github.com/googleapis/google-cloud-cpp/pull/10421))
- fix(storage): scopes should disable self-signed JWTs
  ([#10369](https://github.com/googleapis/google-cloud-cpp/pull/10369))
- doc(storage): document all `oauth2` names as deprecated
  ([#10352](https://github.com/googleapis/google-cloud-cpp/pull/10352))

### [Text-to-Speech](/google/cloud/texttospeech/README.md)

- fix: remove duplicate protos
  ([#10486](https://github.com/googleapis/google-cloud-cpp/pull/10486))

### [Trace](/google/cloud/trace/README.md)

- fix: remove duplicate protos
  ([#10486](https://github.com/googleapis/google-cloud-cpp/pull/10486))

### [Common Libraries](/google/cloud/README.md)

- fix: add missing <cstdint> includes
  ([#10421](https://github.com/googleapis/google-cloud-cpp/pull/10421))
- feat(common): support external accounts
  ([#10465](https://github.com/googleapis/google-cloud-cpp/pull/10465))
  ([#10430](https://github.com/googleapis/google-cloud-cpp/pull/10430))
  ([#10357](https://github.com/googleapis/google-cloud-cpp/pull/10357))
- feat(common): options for `Make*Credentials()`
  ([#10417](https://github.com/googleapis/google-cloud-cpp/pull/10417))
- feat: support logging for unified Rest credentials
  ([#10412](https://github.com/googleapis/google-cloud-cpp/pull/10412))

## v2.5.0 - 2022-12

**NOTE**

- feat!: We have dropped the experimental marker from bidirectional streaming
  APIs ([#10340](https://github.com/googleapis/google-cloud-cpp/pull/10340)).
  The APIs in question are:
  - `bigquery::BigQueryWriteClient::AsyncAppendRows()`
  - `dialogflow_cx::SessionsClient::AsyncStreamingDetectIntent()`
  - `dialogflow_es::ParticipantsClient::AsyncStreamingAnalyzeContent()`
  - `dialogflow_es::SessionsClient::AsyncStreamingDetectIntent()`
  - `logging::LoggingServiceV2Client::AsyncTailLogEntries()`
  - `speech::SpeechClient::AsyncStreamingRecognize()`

If you use any of these APIs, you must drop the `ExperimentalTag` in your code,
accordingly.

### New Libraries

We are happy to announce the following GA library. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Connectors API](/google/cloud/connectors/README.md)
- [VMware Engine API](/google/cloud/vmwareengine/README.md)

### [Bigtable](/google/cloud/bigtable/README.md)

- samples(bigtable): build admin samples with cmake
  ([#10246](https://github.com/googleapis/google-cloud-cpp/pull/10246))
- doc(bigtable): create page for configuration options
  ([#10197](https://github.com/googleapis/google-cloud-cpp/pull/10197))

### [Logging](/google/cloud/logging/README.md)

- feat(logging): generate `AsyncWriteLogEntries()`
  ([#10194](https://github.com/googleapis/google-cloud-cpp/pull/10194))

### [Pub/Sub](/google/cloud/pubsub/README.md)

- feat(pubsub): add option to override subscription
  ([#10327](https://github.com/googleapis/google-cloud-cpp/pull/10327))
- feat(pubsub): blocking pulls
  ([#10317](https://github.com/googleapis/google-cloud-cpp/pull/10317))
- doc(pubsub): create page for configuration options
  ([#10198](https://github.com/googleapis/google-cloud-cpp/pull/10198))

### [Spanner](/google/cloud/spanner/README.md)

- doc(spanner): deprecate old MakeConnection() overloads
  ([#10284](https://github.com/googleapis/google-cloud-cpp/pull/10284))
- fix(spanner): tweak the tag name of a FGAC sample
  ([#10266](https://github.com/googleapis/google-cloud-cpp/pull/10266))
- samples(spanner): build admin samples with cmake
  ([#10247](https://github.com/googleapis/google-cloud-cpp/pull/10247))
- feat(spanner): tests and samples for DML RETURNING
  ([#10233](https://github.com/googleapis/google-cloud-cpp/pull/10233))
- doc(spanner): create page for configuration options
  ([#10199](https://github.com/googleapis/google-cloud-cpp/pull/10199))

### [Speech](/google/cloud/speech/README.md)

- feat(speech): generate speech v2
  ([#10228](https://github.com/googleapis/google-cloud-cpp/pull/10228))

### [Storage](/google/cloud/storage/README.md)

- doc(storage): create page for configuration options
  ([#10200](https://github.com/googleapis/google-cloud-cpp/pull/10200))

### [Common Libraries](/google/cloud/README.md)

- doc: another pass on authentication components
  ([#10300](https://github.com/googleapis/google-cloud-cpp/pull/10300))
- fix(common): create default gRPC credentials only if needed
  ([#10280](https://github.com/googleapis/google-cloud-cpp/pull/10280))
- doc: group client functions
  ([#10268](https://github.com/googleapis/google-cloud-cpp/pull/10268))
- doc: use qualified client name in samples
  ([#10241](https://github.com/googleapis/google-cloud-cpp/pull/10241))
- fix(common): avoid globals for easier DLLs
  ([#10212](https://github.com/googleapis/google-cloud-cpp/pull/10212))
- feat: support a pre-release component of the version string
  ([#10181](https://github.com/googleapis/google-cloud-cpp/pull/10181))
- doc(common): add Doxygen group for common options
  ([#10192](https://github.com/googleapis/google-cloud-cpp/pull/10192))
- doc(common): add overview section
  ([#10193](https://github.com/googleapis/google-cloud-cpp/pull/10193))
- doc: better guidance for authentication samples
  ([#10184](https://github.com/googleapis/google-cloud-cpp/pull/10184))

## v2.4.0 - 2022-11

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Certificate Manager API](/google/cloud/certificatemanager/README.md)
- [Cloud Deploy API](/google/cloud/deploy/README.md)
- [Datastream API](/google/cloud/datastream/README.md)

In addition, these existing libraries are now GA:

- [Cloud Batch](/google/cloud/batch/README.md)

### [BigQuery](/google/cloud/bigquery/README.md)

- doc: add endpoint override snippets to generated libs
  ([#10129](https://github.com/googleapis/google-cloud-cpp/pull/10129))
- feat(bigquery): add migration service
  ([#10034](https://github.com/googleapis/google-cloud-cpp/pull/10034))

### [Bigtable](/google/cloud/bigtable/README.md)

- doc(bigtable): add `*Client` samples
  ([#10149](https://github.com/googleapis/google-cloud-cpp/pull/10149))
- feat(bigtable): support `GOOGLE_CLOUD_ENABLE_DIRECT_PATH`
  ([#9978](https://github.com/googleapis/google-cloud-cpp/pull/9978))

### [IAM](/google/cloud/iam/README.md)

- doc: add endpoint override snippets to generated libs
  ([#10129](https://github.com/googleapis/google-cloud-cpp/pull/10129))

### [Pub/Sub](/google/cloud/pubsub/README.md)

- doc(pubsub): samples for endpoint and auth
  ([#10136](https://github.com/googleapis/google-cloud-cpp/pull/10136))
- feat(pubsub): install pubsub_mocks pkg
  ([#10008](https://github.com/googleapis/google-cloud-cpp/pull/10008))
- feat(pubsub): implement blocking publisher
  ([#10055](https://github.com/googleapis/google-cloud-cpp/pull/10055))
- feat(pubsub): implement per-call options for `Subscriber`
  ([#10043](https://github.com/googleapis/google-cloud-cpp/pull/10043))
- fix(pubsub): limit `ModifyAckDeadlineRequest` size
  ([#10032](https://github.com/googleapis/google-cloud-cpp/pull/10032))
- fix(pubsub): faster shutdowns for `Publisher`
  ([#9991](https://github.com/googleapis/google-cloud-cpp/pull/9991))

### [Spanner](/google/cloud/spanner/README.md)

- doc(spanner): add `*Client` samples
  ([#10145](https://github.com/googleapis/google-cloud-cpp/pull/10145))
- feat(spanner): add support for RowStream::RowsModified()
  ([#10102](https://github.com/googleapis/google-cloud-cpp/pull/10102))
- feat(spanner): support for the PG.JSONB data type
  ([#10098](https://github.com/googleapis/google-cloud-cpp/pull/10098))

### [Storage](/google/cloud/storage/README.md)

- doc(storage): common initialization examples
  ([#10107](https://github.com/googleapis/google-cloud-cpp/pull/10107))
- fix(rest): too many debug headers
  ([#10054](https://github.com/googleapis/google-cloud-cpp/pull/10054))
- fix(rest): return complete payloads for errors
  ([#10051](https://github.com/googleapis/google-cloud-cpp/pull/10051))
- feat(storage): support `Autoclass` feature
  ([#10003](https://github.com/googleapis/google-cloud-cpp/pull/10003))
- feat(storage): faster `InsertObject()` uploads
  ([#9997](https://github.com/googleapis/google-cloud-cpp/pull/9997))
- fix(storage): respect MIME message boundary size limits
  ([#9965](https://github.com/googleapis/google-cloud-cpp/pull/9965))

### [Common Libraries](/google/cloud/README.md)

- feat(generator): generate authentication example
  ([#10138](https://github.com/googleapis/google-cloud-cpp/pull/10138))
- doc: workaround Doxygen formatting quirk
  ([#10137](https://github.com/googleapis/google-cloud-cpp/pull/10137))
- doc: add endpoint override snippets to generated libs
  ([#10129](https://github.com/googleapis/google-cloud-cpp/pull/10129))
- feat(generator): generate simple samples for `*Client`
  ([#10118](https://github.com/googleapis/google-cloud-cpp/pull/10118))
- feat: add mock library w/ StreamRange
  ([#9998](https://github.com/googleapis/google-cloud-cpp/pull/9998))
- doc(common): make authentication docs easier to find
  ([#10110](https://github.com/googleapis/google-cloud-cpp/pull/10110))
- fix: configure context in async retries
  ([#10100](https://github.com/googleapis/google-cloud-cpp/pull/10100))
- doc: document when GrpcNumChannelsOption applies
  ([#10000](https://github.com/googleapis/google-cloud-cpp/pull/10000))
- feat(generator): make idempotency policy non-abstract
  ([#9981](https://github.com/googleapis/google-cloud-cpp/pull/9981))
- fix(common): preserve `ErrorInfo` on retry errors
  ([#9971](https://github.com/googleapis/google-cloud-cpp/pull/9971))

## v2.3.0 - 2022-10

### New Libraries

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

- [Distributed Cloud Edge Container API](/google/cloud/edgecontainer/README.md)
- [Network Connectivity API](/google/cloud/networkconnectivity/README.md)

### [BigQuery](/google/cloud/bigquery/README.md)

The library has been expanded to include [Analytics Hub][bq-analytics-hub], an
API that facilitates data sharing within and across organizations.
([#9882](https://github.com/googleapis/google-cloud-cpp/pull/9882))

### [Spanner](/google/cloud/spanner/README.md)

- fix(spanner): remove session from pool upon "not found" refresh failure
  ([#9954](https://github.com/googleapis/google-cloud-cpp/pull/9954))
- feat(spanner): add support for Customer Managed Multi-Region (CMMR) read-only
  replicas at instance creation time
  ([#9872](https://github.com/googleapis/google-cloud-cpp/pull/9872))

### [Storage](/google/cloud/storage/README.md)

- feat(storage): easier mocks for `HmacKeyMetadata`
  ([#9949](https://github.com/googleapis/google-cloud-cpp/pull/9949))
- feat(storage): easier mocks for `*AccessControl`
  ([#9910](https://github.com/googleapis/google-cloud-cpp/pull/9910))
- feat(storage): easier mocks with `ObjectMetadata`
  ([#9899](https://github.com/googleapis/google-cloud-cpp/pull/9899))
- fix(storage): decay type before testing supported-options membership
  ([#9893](https://github.com/googleapis/google-cloud-cpp/pull/9893))
- feat(storage): easier mocks with `BucketMetadata`
  ([#9886](https://github.com/googleapis/google-cloud-cpp/pull/9886))
- fix(storage): error message for resumable uploads
  ([#9855](https://github.com/googleapis/google-cloud-cpp/pull/9855))
- feat(storage): release `*StallMinimumThroughputOption`
  ([#9813](https://github.com/googleapis/google-cloud-cpp/pull/9813))
- fix(storage): no workaround needed with libc++ and MSVC
  ([#9768](https://github.com/googleapis/google-cloud-cpp/pull/9768))

### [Common Libraries](/google/cloud/README.md)

- fix(common): better defaults for curl initialization
  ([#9798](https://github.com/googleapis/google-cloud-cpp/pull/9798))

## v2.2.0 - 2022-09

### New Libraries

We are introducing new client libraries for GCP services. While we do not
anticipate any API changes to these libraries before declaring them GA, we are
releasing them early in case they elicit some feedback that requires changes.

- [API Keys](/google/cloud/apikeys/README.md)

We are happy to announce the following GA libraries. Unless specifically noted,
the APIs in these libraries are stable, and are ready for production use.

<details>
<summary> Expand to see the full list of new GA libraries...</summary>
<br>

- [Bare Metal Solution](/google/cloud/baremetalsolution/README.md)
- [Beyond Corp](/google/cloud/beyondcorp/README.md)
- [Optimization AI](/google/cloud/optimization/README.md)
- [Cloud Run](/google/cloud/run/README.md)
- [Video Services](/google/cloud/video/README.md)

</details>

### [Bigtable](/google/cloud/bigtable/README.md)

- fix(bigtable): `DataConnection` refreshes channels
  ([#9718](https://github.com/googleapis/google-cloud-cpp/pull/9718))
- fix(bigtable): Use retry policy on all streams with failing mutations
  ([#9706](https://github.com/googleapis/google-cloud-cpp/pull/9706))
- feat(bigtable): per-operation Options
  ([#9627](https://github.com/googleapis/google-cloud-cpp/pull/9627))
  ([#9623](https://github.com/googleapis/google-cloud-cpp/pull/9623))

### [Dataproc](/google/cloud/dataproc/README.md)

- feat(dataproc): mark the dataproc services as location dependent
  ([#9722](https://github.com/googleapis/google-cloud-cpp/pull/9722))

### [Spanner](/google/cloud/spanner/README.md)

- feat(spanner): fine-grained access control
  ([#9669](https://github.com/googleapis/google-cloud-cpp/pull/9669))
- feat(spanner): equality for copyable classes
  ([#9648](https://github.com/googleapis/google-cloud-cpp/pull/9648))

### [Storage](/google/cloud/storage/README.md)

- We have a new implementation for HTTP requests. This new implementation
  provides comparable download performance, and improves some uploads. See
  [#9659] for details. We have rigorously tested this new implementation. In the
  unlikely event that this new implementation breaks your application, we have
  included an environment variable to revert to the legacy implementation. Set
  `GOOGLE_CLOUD_CPP_STORAGE_USE_LEGACY_HTTP` to any value to use the legacy
  implementation. We are planning to remove the legacy code and the
  `GOOGLE_CLOUD_CPP_STORAGE_USE_LEGACY_HTTP` environment variable by 2022-12.
- feat(storage): improve error messages on stalled uploads
  ([#9744](https://github.com/googleapis/google-cloud-cpp/pull/9744))
- feat(storage): equality for Native IAM types
  ([#9649](https://github.com/googleapis/google-cloud-cpp/pull/9649))
- feat(storage): SA credentials default to self-signed JWTs
  ([#9629](https://github.com/googleapis/google-cloud-cpp/pull/9629))

### [Common Libraries](/google/cloud/README.md)

- feat: group GUAC `Options` in an `OptionsList`
  ([#9643](https://github.com/googleapis/google-cloud-cpp/pull/9643))
- fix(rest): cache credentials
  ([#9620](https://github.com/googleapis/google-cloud-cpp/pull/9620))
- doc: add table of contents to landing page
  ([#9671](https://github.com/googleapis/google-cloud-cpp/pull/9671))

## v2.1.0 - 2022-08

### New Libraries

We are introducing new client libraries for GCP services. While we do not
anticipate any API changes to these libraries before declaring them GA, we are
releasing them early in case they elicit some feedback that requires changes.

- [Batch](/google/cloud/batch/README.md)
  ([#9474](https://github.com/googleapis/google-cloud-cpp/pull/9474))
- [Beyond Corp](/google/cloud/beyondcorp/README.md)
  ([#9555](https://github.com/googleapis/google-cloud-cpp/pull/9555))
- [Cloud Run](/google/cloud/run/README.md)
  ([#9460](https://github.com/googleapis/google-cloud-cpp/pull/9460))

### [Assured Workloads](/google/cloud/assuredworkloads/README.md)

- fix(assuredworkloads): reenable on windows
  ([#9467](https://github.com/googleapis/google-cloud-cpp/pull/9467))

### [Bigtable](/google/cloud/bigtable/README.md)

- fix(bigtable): the mocks library is no longer header-only
  ([#9568](https://github.com/googleapis/google-cloud-cpp/pull/9568))

### [Cloud Asset](/google/cloud/asset/README.md)

- fix(asset): reenable on macOS
  ([#9468](https://github.com/googleapis/google-cloud-cpp/pull/9468))

### [Pub/Sub](/google/cloud/pubsub/README.md)

- fix(pubsub): missing subscription name in lease extensions
  ([#9523](https://github.com/googleapis/google-cloud-cpp/pull/9523))
- feat(pubsub): exactly-once delivery
  ([#9436](https://github.com/googleapis/google-cloud-cpp/pull/9436))

### [Spanner](/google/cloud/spanner/README.md)

- fix(spanner): avoid evaluation-order issue in function arguments
  ([#9452](https://github.com/googleapis/google-cloud-cpp/pull/9452))

### [Stackdriver Debugger](/google/cloud/debugger/README.md)

- doc(debugger): announce deprecation
  ([#9552](https://github.com/googleapis/google-cloud-cpp/pull/9552))

### [Storage](/google/cloud/storage/README.md)

- feat(storage): experimental options to tune stall timeouts
  ([#9593](https://github.com/googleapis/google-cloud-cpp/pull/9593))
- feat(storage): add debugging headers to `ObjectWriteStream`
  ([#9580](https://github.com/googleapis/google-cloud-cpp/pull/9580))
- fix(storage): no char for `std::uniform_int_distribution`
  ([#9509](https://github.com/googleapis/google-cloud-cpp/pull/9509))
- feat(storage): support Bucket custom placement config
  ([#9481](https://github.com/googleapis/google-cloud-cpp/pull/9481))

### [Common Libraries](/google/cloud/README.md)

- feat(common): make the RPC log even more readable
  ([#9561](https://github.com/googleapis/google-cloud-cpp/pull/9561))
- feat: improve error messages for access token errors
  ([#9485](https://github.com/googleapis/google-cloud-cpp/pull/9485))
- feat(common): make the RPC log even more readable
  ([#9477](https://github.com/googleapis/google-cloud-cpp/pull/9477))

## v2.0.0 - 2022-07

**BREAKING CHANGES**

As previously announced, `google-cloud-cpp` now requires C++ >= 14. This is
motivated by similar changes in our dependencies, and because C++ 14 has been
the default C++ version in all the compilers we support for several years.

We think this change is large enough that deserves a major version bump to
signal the new requirements.

If you are already using C++ >= 14 you need to make no changes. If you are using
C++11: please consider updating as soon as possible. To ease your transition to
C++ >= 14 we will, if requested, backport critical fixes to v1.42.0 until
2023-07-01. After 2023-07-01 we will drop all support to v1.42.0 and earlier
versions.

**Debian 9 (Stretch) is EOL**

Debian 9 (Stretch) reached EOL on 2022-06-30. Therefore, we have stopped testing
or supporting this distribution. This was the last distribution we supported
that required GCC \< 7.3, and/or CMake \< 3.10. Starting with this release we
require CMake >= 3.10, and only test with GCC >= 7.3.

**OTHER CHANGES**

### [Bigtable](/google/cloud/bigtable/README.md)

We introduced a [new constructor][modern-table-ctor] for `Table` which accepts a
`DataConnection` instead of a `DataClient`. The `DataConnection` is a new
interface that more closely matches the client surface of `Table`. Read more
about `*Connection` classes in our
[Architecture Design][architecture-connection] document.

#### What are the benefits of `DataConnection`?

The new API greatly simplifies mocking. Every `Table::Foo(..)` call has an
associated `DataConnection::Foo(...)` call. This allows you to set expectations
on the exact values returned by the client call. See
[Mocking the Cloud Bigtable C++ Client][howto-mock-data-api] for a complete
example on how to mock the behavior of `Table` with
`bigtable_mocks::MockDataConnection`.

The new `DataConnection` API offers more consistency across our libraries. It
also enables the use of some common library features, such as our
[`UnifiedCredentialsOption`][guac-dox]. Also, any new features will be added to
the `DataConnection` API first.

#### Do I need to update my code?

No. If the benefits are not appealing enough, you do not need to update your
code. All code that currently uses `DataClient` will continue to function as
before. This includes uses of `testing::MockDataClient`.

However, if you are using `testing::MockDataClient` to mock the behavior of
`Table` in your tests:

1. Be aware that we have announced our intention to remove classes derived from
   `DataClient` on or around 2023-05. Your tests will break then.
1. Please consider using `bigtable_mocks::MockDataConnection`. It will greatly
   simplify your tests.

#### How do I update existing `DataClient` code?

See [Migrating from `DataClient` to `DataConnection`][cbt-dataclient-migration].

- doc(bigtable): how to mock the Data API
  ([#9415](https://github.com/googleapis/google-cloud-cpp/pull/9415))
- feat(bigtable): modern `Table` constructor
  ([#9403](https://github.com/googleapis/google-cloud-cpp/pull/9403))
- feat(generator): support explicit routing headers
  ([#9368](https://github.com/googleapis/google-cloud-cpp/pull/9368))
- fix(bigtable)!: pass app profile id to connection as options
  ([#9388](https://github.com/googleapis/google-cloud-cpp/pull/9388))
- feat(bigtable): add `AppProfileIdOption`
  ([#9382](https://github.com/googleapis/google-cloud-cpp/pull/9382))
- feat(bigtable): table resource name as a class
  ([#9377](https://github.com/googleapis/google-cloud-cpp/pull/9377))
- feat(bigtable): instance name as a class
  ([#9374](https://github.com/googleapis/google-cloud-cpp/pull/9374))
- feat(bigtable): introduce `MockDataConnection` and `MakeTestRowReader`
  ([#9335](https://github.com/googleapis/google-cloud-cpp/pull/9335))
- feat(bigtable): introduce `DataConnection`
  ([#9323](https://github.com/googleapis/google-cloud-cpp/pull/9323))
- feat(bigtable): modern Data API policy options
  ([#9320](https://github.com/googleapis/google-cloud-cpp/pull/9320))

### [Pub/Sub](/google/cloud/pubsub/README.md)

- doc(pubsub): improve documentation for `*AckHandler`
  ([#9404](https://github.com/googleapis/google-cloud-cpp/pull/9404))
- feat(pubsub): update subscription builders
  ([#9326](https://github.com/googleapis/google-cloud-cpp/pull/9326))

### [Common Libraries](/google/cloud/README.md)

- fix(generator): handle explicit routing params for nested fields
  ([#9408](https://github.com/googleapis/google-cloud-cpp/pull/9408))
- feat(common): truncation support for plain strings in the RPC log
  ([#9351](https://github.com/googleapis/google-cloud-cpp/pull/9351))

### New Libraries

We are introducing a new client library. While we do not anticipate any API
changes to this library before declaring it GA, we are releasing it early in
case it elicits some feedback that requires changes.

- [Video Services](/google/cloud/video/README.md)

[#10170]: https://github.com/googleapis/google-cloud-cpp/issues/10170
[#10174]: https://github.com/googleapis/google-cloud-cpp/discussions/10174
[#8022]: https://github.com/googleapis/google-cloud-cpp/issues/8022
[#9659]: https://github.com/googleapis/google-cloud-cpp/issues/9659
[architecture-connection]: /ARCHITECTURE.md#the-connection-classes
[bq-analytics-hub]: https://cloud.google.com/bigquery/docs/analytics-hub-introduction
[cbt-dataclient-migration]: https://cloud.google.com/cpp/docs/reference/bigtable/latest/migrating-from-dataclient
[cloud-debugger-deprecated]: https://cloud.google.com/debugger/docs/deprecations
[cloud-iot-shutdown]: https://cloud.google.com/iot/docs/release-notes#August_16_2022
[cloud-run-jobs]: https://cloud.google.com/run/docs/managing/job-executions
[dataproc-node-groups]: https://cloud.google.com/dataproc/docs/guides/node-groups/dataproc-driver-node-groups
[distributed tracing]: https://opentelemetry.io/docs/concepts/observability-primer/#distributed-traces
[functions-v2]: https://cloud.google.com/functions/docs/concepts/version-comparison
[google.pubsub.v1.schemaserviceclient]: https://cloud.google.com/pubsub/docs/reference/rpc/google.pubsub.v1#google.pubsub.v1.SchemaService
[grpc#34482]: https://github.com/grpc/grpc/issues/34482
[guac-dox]: https://cloud.google.com/cpp/docs/reference/common/latest/group__guac
[howto-mock-data-api]: https://cloud.google.com/cpp/docs/reference/bigtable/latest/bigtable-mocking
[logging-config]: https://cloud.google.com/logging/docs/routing/overview
[logging-metrics]: https://cloud.google.com/logging/docs/logs-based-metrics
[modern-table-ctor]: https://github.com/googleapis/google-cloud-cpp/blob/62740c8e9180056db77d4dd3e80a6fa7ae71295a/google/cloud/bigtable/table.h#L182-L214
[monitoring-snooze]: https://cloud.google.com/monitoring/alerts/snooze
[opentelemetry]: https://opentelemetry.io/
[oss-cxx-support]: https://opensource.google/documentation/policies/cplusplus-support
[otel-quickstart]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/opentelemetry/quickstart
[product-launch-stages]: https://cloud.google.com/products/#product-launch-stages
[resource-manager-tags]: https://cloud.google.com/resource-manager/docs/tags/tags-overview
[speech-model-adaptation]: https://cloud.google.com/speech-to-text/docs/adaptation-model
