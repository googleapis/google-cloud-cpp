# Changelog

## FUTURE BREAKING CHANGES:

<!-- Keep these sorted by estimated date -->

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

<details>
<summary>2022-10-01: retiring legacy Spanner admin APIs</summary>
<br>

* On 2022-10-01 (or shortly after) we are planning to remove the hand-written
  versions of the Spanner admin APIs. These have been superseded by versions
  generated automatically from the service definitions. The new APIs can be
  found in the [`google/cloud/spanner/admin`](https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/spanner/admin)
  tree and within the `google::cloud::spanner_admin` namespace. Starting with
  the v1.32.0 release, and depending on your compiler settings, using the old
  classes/functions may elicit a deprecation warning. See
  [#7356](https://github.com/googleapis/google-cloud-cpp/issues/7356) for more
  details.
</details>

<details>
<summary>2022-11-01: remove `v1` and `gcpcxxV1` backward compatibility aliases</summary>
<br>

* On 2022-11-01 (or shortly after) we will remove the inline namespace aliases
  of `v1` and `gcpcxxV1` that are declared in `google/cloud/version.h`. These
  aliases exist because we changed the format of our inline namespace name to
  include the major, minor, and patch numbers for each release, and we didn't
  want to break users. Our Doxygen documentation was mistakenly recommending
  that users include the inline namespace names in their code, but this was
  also fixed to now recommend against doing so. Users should generally omit the
  name of our versioned inline namespace name because that will tightly couple
  their code to a specific release, and will make upgrading to newer versions
  more difficult. Instead, users should simply omit the inline namespace name,
  for example, instead of ~`google::cloud::v1::Status`~ use
  `google::cloud::Status`. Please update your code to avoid naming the `v1` and
  `gcpcxxV1` inline namespace names before 2022-11-01. For more info, see
  https://github.com/googleapis/google-cloud-cpp/issues/7463 and
  https://github.com/googleapis/google-cloud-cpp/issues/5976.
</details>

<details>
<summary>2023-02-01: remove `BigQueryReadReadRowsStreamingUpdater` from
`google::cloud::bigquery` namespace</summary>
<br>

* On 2023-02-01 (or shortly after) we will remove
`BigQueryReadReadRowsStreamingUpdater` from its declaration in
`google/cloud/bigquery/read_connection.h` and from the `google::cloud::bigquery`
namespace. The function continues to exist but in an internal file and
namespace. For status on this see
https://github.com/googleapis/google-cloud-cpp/issues/8234.
</details>

## v1.39.0 - TBD

**BREAKING CHANGES**

* Starting with this release Linux and Windows builds based on CMake
  no longer set `CMAKE_CXX_STANDARD` to `11`.  We rely on the compiler's
  default C++ language standard. Note that all the compilers we support default
  to C++14.
  * On macOS the default is C++98, as `google-cloud-cpp` requires C++ >= 11, we continue
    to set `CMAKE_CXX_STANDARD` on that platform.
  * This only changes the default C++ version, we continue to test and support C++11.
  * For more details, see [#6767](https://github.com/googleapis/google-cloud-cpp/issues/6767).

### [Bigtable](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigtable/README.md)

**BREAKING CHANGES**

* `InstanceAdmin::GetIamPolicy` and `InstanceAdmin::SetIamPolicy` have been
  retired. If you are affected by this removal, please use
  `InstanceAdmin::GetNativeIamPolicy` and `InstanceAdmin::SetNativeIamPolicy`
  instead. See [#5929] for more details.

### [KMS](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/kms/README.md)

The library has been expanded to include the following services:

* [External Key Manager](https://cloud.google.com/kms/docs/ekm)

### [Storage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storage/README.md)

**BREAKING CHANGES**

* `Client::GetBucketIamPolicy` and `Client::SetBucketIamPolicy` have been
  retired. If you are affected by this removal, please use
  `Client::GetNativeBucketIamPolicy` and `Client::SetNativeBucketIamPolicy`
  instead. See [#5929] for more details.

## v1.38.0 - 2022-03

### [Service Management API](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/servicemanagement/README.md)

* The `DisableService()` and `EnableService()` RPCs are now retired.  These RPCs
  were deprecated and non-functional at the time this client library was
  generated.  We do not expect this will actually break any existing code.
  Nevertheless, we apologize for any confusion.

### [Pub/Sub Lite](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/pubsublite/README.md)

**BREAKING CHANGES**

* We mistakenly declared GA for this library. Though the current public APIs are
  stable, the library is incomplete. We are still working on the APIs to publish
  and receive messages.  We apologize for any confusion or inconvenience this
  caused.

### [Common Libraries](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/README.md)

* fix(pubsublite)!: revert GA declaration ([#8497](https://github.com/googleapis/google-cloud-cpp/pull/8497))
* fix: no `experimental-` prefix for GA targets ([#8496](https://github.com/googleapis/google-cloud-cpp/pull/8496))
* fix(container): missing CMake dependency ([#8492](https://github.com/googleapis/google-cloud-cpp/pull/8492))

## v1.37.0 - 2022-03

**BREAKING CHANGES**

* As previously announced, we are removing certain legacy CMake targets and
  Bazel rules in this release.
  - **Bazel Users:** applications should use the targets at the top-level
    directory, e.g. `//:bigtable`, or `//:pubsub`.  Targets in each directory
    (e.g. `//google/cloud/bigtable:bigtable_client`) are now retired or marked
    private.
  - **CMake Users:** applications should use the
    `google-cloud-cpp::*` targets (e.g. `google-cloud-cpp::pubsub`).
    - All exported targets without a `google-cloud-cpp::` prefix are retired.
      These include, but are not limited to:
      - Any target starting with `googleapis-c++::`
      - Any exported targets without a prefix, including:
        `google_cloud_cpp_common`, `google_cloud_cpp_grpc_utils`,
        `bigtable_client`, `bigtable_protos`, `firestore_client`,
        `pubsub_client`, `storage_client`, `spanner_client`.
      - Some target aliases, including `bigtable::client`, `bigtable::protos`
  - **pkg-config users:** applications should use the modules starting with
    `google_cloud_cpp`. All other modules are now retired.
  - **Direct users of -l${library} flags:** we do not recommend that
    applications uses `-l` flags directly, please use `pkg-config` and/or
    the target names under CMake or Bazel. We make this recommendation because
    we do not know of any mechanism to provide backwards compatibility for such
    flags.
  - More details about the rationale for these changes in [#5726].

[#5726]: https://github.com/googleapis/google-cloud-cpp/issues/5726

### [BigQuery](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigquery/README.md)

The library has been expanded to include the following services:

* [BigQuery ML](https://cloud.google.com/bigquery-ml/docs)
* [BigQuery Connection API](https://cloud.google.com/bigquery/docs/connections-api-intro)
* [BigQuery Data Transfer Service](https://cloud.google.com/bigquery-transfer)
* [BigQuery Reservations](https://cloud.google.com/bigquery/docs/reservations-intro)
* [BigQuery Storage Write API](https://cloud.google.com/bigquery/docs/write-api)

### [Bigtable](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigtable/README.md)

**BREAKING CHANGE:** The `bigtable::AdminClient` interface has changed
significantly. Any code that extends this class or calls its experimental public
APIs (`reset()`, `Channel()`) will be broken. For the most part, this should
only affect customers who mock this class in their tests. Code that calls
`bigtable::MakeAdminClient()` or `bigtable::CreateDefaultAdminClient()` will
continue to work as before.

This change will allow us to deliver new features more quickly by reducing the
maintenance costs of the library. It also provides a better mocking interface
for `bigtable::TableAdmin`. We apologize if you are inconvenienced by this
change.

If only your tests are broken, please use
`bigtable_admin_mocks::MockBigtableTableAdminConnection` in place of
`bigtable::testing::MockAdminClient`. The new mock should be more intuitive
because of the differences described below:

|`MockBigtableTableAdminConnection` | `MockAdminClient` |
|-|-|
| Mocks result of entire retry loop | Mocks result of one call in a retry loop |
| Returns familiar `google::cloud::` types | Returns `grpc::` types |

If more than your tests are broken, please use
`bigtable_admin::BigtableTableAdminClient` in favor of `bigtable::TableAdmin`,
and `bigtable_admin::BigtableTableAdminConnection` in favor of
`bigtable::AdminClient`. These classes will incorporate the newest features of
both the [Cloud Bigtable Admin API] and the C++ client library. For more
information on these new classes, see our [Architecture Design] document.

Again, we apologize for making this breaking change, but we believe it is in the
best long-term interest of our customers.

**BREAKING CHANGE:** The `bigtable::InstanceAdminClient` class has been marked
as `final`. After the changes in [v1.36.0](#v1360---2022-02), there is no need
or reason to be extending this class.

**OTHER CHANGES**:

* feat(bigtable): better support for PSC and VPC-SC ([#8458](https://github.com/googleapis/google-cloud-cpp/pull/8458))
* fix(bigtable): ReadRows retries from the `last_scanned_row_key` ([#8423](https://github.com/googleapis/google-cloud-cpp/pull/8423))
* feat(bigtable): support x-goog-user-project ([#8324](https://github.com/googleapis/google-cloud-cpp/pull/8324))

### [IAM](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iam/README.md)

* docs(iam): added comments on private_key_data ([#8204](https://github.com/googleapis/google-cloud-cpp/pull/8204))

### [Pub/Sub](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/pubsub/README.md)

* feat(pubsub): support `AuthorityOption` ([#8460](https://github.com/googleapis/google-cloud-cpp/pull/8460))
* feat(pubsub): support x-goog-user-project ([#8456](https://github.com/googleapis/google-cloud-cpp/pull/8456))
* feat(pubsub): per-call Options for SubscriptionAdminClient ([#8414](https://github.com/googleapis/google-cloud-cpp/pull/8414))
* feat(pubsub): per-call options in TopicAdminClient ([#8411](https://github.com/googleapis/google-cloud-cpp/pull/8411))
* feat(pubsub): per-call options in SchemaAdminClient ([#8406](https://github.com/googleapis/google-cloud-cpp/pull/8406))
* doc(pubsub): add region tag for subscription with filter ([#8326](https://github.com/googleapis/google-cloud-cpp/pull/8326))

### [Spanner](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/README.md)

All the `spanner::Client` operations now take optional `google::cloud::Options`
arguments, replacing the existing `ClientOptions`, `CommitOptions`,
`PartitionOptions`, `QueryOptions`, and `ReadOptions` arguments, or adding
options to operations that previously had none. Users should migrate to these
new overloads. (Note that the old `spanner::*Options` types have not been
deprecated as they are still used in the `spanner::Connection` interface.)

**OTHER CHANGES**:

* feat(spanner): use prevailing Options in ConnectionImpl ([#8466](https://github.com/googleapis/google-cloud-cpp/pull/8466))
* feat(spanner): convert ReadOptions/PartitionOptions to Options ([#8448](https://github.com/googleapis/google-cloud-cpp/pull/8448))
* fix(spanner): instantiate OptionsSpan objects in legacy admin calls ([#8440](https://github.com/googleapis/google-cloud-cpp/pull/8440))
* feat(spanner): convert QueryOptions to Options in main client API ([#8430](https://github.com/googleapis/google-cloud-cpp/pull/8430))
* fix(spanner): add Options save/restore to PartialResultSetSource ([#8355](https://github.com/googleapis/google-cloud-cpp/pull/8355))
* feat(spanner): support x-goog-user-project ([#8316](https://github.com/googleapis/google-cloud-cpp/pull/8316))

### [Storage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storage/README.md)

* feat(storage): support AuthorityOption ([#8462](https://github.com/googleapis/google-cloud-cpp/pull/8462))
* fix(storage): json["projectTeam"] might be present but null ([#8446](https://github.com/googleapis/google-cloud-cpp/pull/8446))
* doc(storage): improve `{Bucket,Object}Metadata` docs ([#8434](https://github.com/googleapis/google-cloud-cpp/pull/8434))
* fix(storage): ignored fields in lifecycle patches ([#8389](https://github.com/googleapis/google-cloud-cpp/pull/8389))
* doc(storage): show how to configure endpoints ([#8354](https://github.com/googleapis/google-cloud-cpp/pull/8354))
* doc(storage): UserIp option is deprecated ([#8468](https://github.com/googleapis/google-cloud-cpp/pull/8468))

### [Common Libraries](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/README.md)

* feat(generator): all bidir streams are experimental ([#8471](https://github.com/googleapis/google-cloud-cpp/pull/8471))
* feat: better support for PSC and VPC-SC ([#8453](https://github.com/googleapis/google-cloud-cpp/pull/8453))
* fix: wait until AutomaticallyCreatedBackgroundThreads start ([#8452](https://github.com/googleapis/google-cloud-cpp/pull/8452))
* fix: restore Options over deletion of StreamRange<T>::reader_ ([#8403](https://github.com/googleapis/google-cloud-cpp/pull/8403))
* feat(common): converting constructors for future ([#8329](https://github.com/googleapis/google-cloud-cpp/pull/8329))
* fix: add Options save/restore to StreamRange<T> and cancellations ([#8256](https://github.com/googleapis/google-cloud-cpp/pull/8256))
* feat(generator): support parameters named "options" ([#8283](https://github.com/googleapis/google-cloud-cpp/pull/8283))
* feat(generator): support `x-goog-user-project` ([#8245](https://github.com/googleapis/google-cloud-cpp/pull/8245))
* fix: correct uses of `target_compatible_with` ([#8257](https://github.com/googleapis/google-cloud-cpp/pull/8257))
* fix(generator): Connection base-class operations should fail ([#8236](https://github.com/googleapis/google-cloud-cpp/pull/8236))

### New Libraries

We are introducing client libraries for 9 more GCP services. While we do not
anticipate any API changes to these libraries before declaring them GA, we are
releasing them early in case they elicit some feedback that requires changes.

<details>
<summary> Expand to see the full list of new libraries...</summary>
<br>

* [Apigee Hybrid](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/apigeeconnect/README.md)
* [Cloud Profiler](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/profiler/README.md)
* [Contact Center AI Insights](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/contactcenterinsights/README.md)
* [Data Catalog](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/datacatalog/README.md)
* [Dataproc](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/dataproc/README.md)
* [Document AI](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/documentai/README.md)
* [Managed Service for Microsoft Active Directory](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/managedidentities/README.md)
* [Natural Language AI](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/language/README.md)
* [Resource Settings](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/resourcesettings/README.md)

</details>

We are happy to announce that the following GA libraries.  We expect these
libraries to have a stable API, to offer the full functionality of the
GA version of service they wrap, and are ready for production use.

<details>
<summary> Expand to see the full list of new GA libraries...</summary>
<br>

* [Access Approval](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/accessapproval/README.md)
* [Access Context Manager](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/accesscontextmanager/README.md)
* [Anthos GKE API](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/gkehub/README.md)
* [API Gateway](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/apigateway/README.md)
* [App Engine Admin API](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/appengine/README.md)
* [Artifact Registry](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/artifactregistry/README.md)
* [Assured Workloads](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/assuredworkloads/README.md)
* [AutoML](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/automl/README.md)
* [Binary Authorization](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/binaryauthorization/README.md)
* [Certificate Authority Service](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/privateca/README.md)
* [Channel Services](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/channel/README.md)
* [Cloud Asset Inventory](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/asset/README.md)
* [Cloud Billing](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/billing/README.md)
* [Cloud Build](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/cloudbuild/README.md)
* [Cloud Composer](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/composer/README.md)
* [Cloud Data Loss Prevention (DLP)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/dlp/README.md)
* [Cloud Debugger](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/debugger/README.md)
* [Cloud Functions](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/functions/README.md)
* [Cloud Intrusion Detection System (IDS)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/ids/README.md)
* [Cloud IoT](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iot/README.md)
* [Cloud Key Management Service (KMS)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/kms/README.md)
* [Cloud Scheduler](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/scheduler/README.md)
* [Cloud Shell](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/shell/README.md)
* [Cloud TPU](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/tpu/README.md)
* [Cloud Trace](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/trace/README.md)
* [Cloud Translation](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/translate/README.md)
* [Cloud Vision](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/vision/README.md)
* [Compute Engine OS Config](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/osconfig/README.md)
* [Compute Engine OS Login](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/oslogin/README.md)
* [Connectivity Tests](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/networkmanagement/README.md)
* [Container Analysis](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/containeranalysis/README.md)
* [Database Migration Service (DMS)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/datamigration/README.md)
* [Eventarc](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/eventarc/README.md)
* [Filestore](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/filestore/README.md)
* [Game Servers](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/gameservices/README.md)
* [Google Kubernetes Engine (GKE)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/container/README.md)
* [Identity-Aware Proxy (IAP)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iap/README.md)
* [Memorystore for Memcached](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/memcache/README.md)
* [Memorystore for Redis](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/redis/README.md)
* [Migrate for Compute Engine](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/vmmigration/README.md)
* [Organization Policy Service](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/orgpolicy/README.md)
* [Policy Troubleshooter](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/policytroubleshooter/README.md)
* [Recommender](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/recommender/README.md)
* [Resource Manager](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/resourcemanager/README.md)
* [Retail](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/retail/README.md)
* [Security Command Center](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/securitycenter/README.md)
* [Serverless VPC Access](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/vpcaccess/README.md)
* [Service Control](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/servicecontrol/README.md)
* [Service Directory](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/servicedirectory/README.md)
* [Service Management](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/servicemanagement/README.md)
* [Service Usage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/serviceusage/README.md)
* [Storage Transfer Service](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storagetransfer/README.md)
* [Talent Solution](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/talent/README.md)
* [Text-to-Speech](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/texttospeech/README.md)
* [Vertex AI Workbench](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/notebooks/README.md)
* [Video Intelligence API](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/videointelligence/README.md)
* [Web Risk](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/webrisk/README.md)
* [Web Security Scanner](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/websecurityscanner/README.md)
* [Workflows](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/workflows/README.md)
</details>

## v1.36.0 - 2022-02

### Bazel support

**BREAKING CHANGE:** We stopped testing with Bazel 3.5 and moved most of our
Bazel tests to Bazel 4.0, which is now our minimum supported Bazel version. We
also added a `bazel-latest` build to ensure that we always work with the newest
Bazel release (currently 5.0). For more details on the changes, see [#8095] and
[#8099]. For more info about Bazel, see https://bazel.build/.

[#8095]: https://github.com/googleapis/google-cloud-cpp/pull/8095
[#8099]: https://github.com/googleapis/google-cloud-cpp/pull/8099

### [BigQuery](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigquery/README.md)

* feat(generator): merge connection options into client options ([#8158](https://github.com/googleapis/google-cloud-cpp/pull/8158))
* feat(generator): connection respects per call policies ([#8013](https://github.com/googleapis/google-cloud-cpp/pull/8013))
* fix: add mock library aliases ([#7844](https://github.com/googleapis/google-cloud-cpp/pull/7844))

### [Bigtable](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigtable/README.md)

**BREAKING CHANGE:** The `bigtable::InstanceAdminClient` interface has changed
significantly. Any code that extends this class or calls its experimental public
APIs (`reset()`, `Channel()`) will be broken. For the most part, this should
only affect customers who mock this class in their tests. Code that calls
`bigtable::MakeInstanceAdminClient()` or
`bigtable::CreateDefaultInstanceAdminClient()` will continue to work as before.

This change will allow us to deliver new features more quickly by reducing the
maintenance costs of the library. It also provides a better mocking interface
for `bigtable::InstanceAdmin`. We apologize if you are inconvenienced by this
change.

If only your tests are broken, please use
`bigtable_admin_mocks::MockBigtableInstanceAdminConnection` in place of
`bigtable::testing::MockInstanceAdminClient`. The new mock should be more
intuitive because of the differences described below:

|`MockBigtableInstanceAdminConnection` | `MockInstanceAdminClient` |
|-|-|
| Mocks result of entire retry loop | Mocks result of one call in a retry loop |
| Returns familiar `google::cloud::` types | Returns `grpc::` types |

If more than your tests are broken, please use
`bigtable_admin::BigtableInstanceAdminClient` in favor of
`bigtable::InstanceAdmin`, and `bigtable_admin::BigtableInstanceAdminConnection`
in favor of `bigtable::InstanceAdminClient`. These classes will incorporate the
newest features of both the [Cloud Bigtable Admin API] and the C++ client
library. For more information on these new classes, see our
[Architecture Design] document.

Again, we apologize for making this breaking change, but we believe it is in the
best long-term interest of our customers.

**OTHER CHANGES**:

* fix(generator): fix options handling in SetIamPolicy() OCC loop ([#8203](https://github.com/googleapis/google-cloud-cpp/pull/8203))
* feat(bigtable): cheap Admin creation with different resource name ([#8194](https://github.com/googleapis/google-cloud-cpp/pull/8194))
* feat(bigtable): cheap Table creation with different resource name ([#8172](https://github.com/googleapis/google-cloud-cpp/pull/8172))
* feat(generator): merge connection options into client options ([#8158](https://github.com/googleapis/google-cloud-cpp/pull/8158))
* feat(generator): connection respects per call policies ([#8013](https://github.com/googleapis/google-cloud-cpp/pull/8013))
* fix(bigtable): polling policy clones initial state ([#7854](https://github.com/googleapis/google-cloud-cpp/pull/7854))
* fix: add mock library aliases ([#7844](https://github.com/googleapis/google-cloud-cpp/pull/7844))

[Cloud Bigtable Admin API]: https://cloud.google.com/bigtable/docs/reference/admin/rpc
[Architecture Design]: https://github.com/googleapis/google-cloud-cpp/blob/main/ARCHITECTURE.md#the-client-classes

### [Cloud Tasks](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/tasks/README.md)

* fix(generator): fix options handling in SetIamPolicy() OCC loop ([#8203](https://github.com/googleapis/google-cloud-cpp/pull/8203))
* feat(generator): merge connection options into client options ([#8158](https://github.com/googleapis/google-cloud-cpp/pull/8158))
* feat(generator): connection respects per call policies ([#8013](https://github.com/googleapis/google-cloud-cpp/pull/8013))
* fix: add mock library aliases ([#7844](https://github.com/googleapis/google-cloud-cpp/pull/7844))

### [IAM](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iam/README.md)

* fix(generator): fix options handling in SetIamPolicy() OCC loop ([#8203](https://github.com/googleapis/google-cloud-cpp/pull/8203))
* feat(generator): merge connection options into client options ([#8158](https://github.com/googleapis/google-cloud-cpp/pull/8158))
* feat(generator): connection respects per call policies ([#8013](https://github.com/googleapis/google-cloud-cpp/pull/8013))
* fix: add mock library aliases ([#7844](https://github.com/googleapis/google-cloud-cpp/pull/7844))

### [Pub/Sub](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/pubsub/README.md)

* fix: add mock library aliases ([#7844](https://github.com/googleapis/google-cloud-cpp/pull/7844))

### [Secret Manager](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/secretmanager/README.md)

* fix(generator): fix options handling in SetIamPolicy() OCC loop ([#8203](https://github.com/googleapis/google-cloud-cpp/pull/8203))
* feat(generator): merge connection options into client options ([#8158](https://github.com/googleapis/google-cloud-cpp/pull/8158))
* feat(generator): connection respects per call policies ([#8013](https://github.com/googleapis/google-cloud-cpp/pull/8013))
* fix: add mock library aliases ([#7844](https://github.com/googleapis/google-cloud-cpp/pull/7844))

### [Spanner](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/README.md)

* fix(generator): fix options handling in SetIamPolicy() OCC loop ([#8203](https://github.com/googleapis/google-cloud-cpp/pull/8203))
* feat(generator): merge connection options into client options ([#8158](https://github.com/googleapis/google-cloud-cpp/pull/8158))
* feat(spanner): merge connection options into client options ([#8090](https://github.com/googleapis/google-cloud-cpp/pull/8090))
* fix(spanner): apply policy options in (generated) client ctor ([#8064](https://github.com/googleapis/google-cloud-cpp/pull/8064))
* feat(generator): connection respects per call policies ([#8013](https://github.com/googleapis/google-cloud-cpp/pull/8013))
* fix: add mock library aliases ([#7844](https://github.com/googleapis/google-cloud-cpp/pull/7844))
* feat(spanner): support unified credentials ([#7824](https://github.com/googleapis/google-cloud-cpp/pull/7824))

### [Storage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storage/README.md)

**BREAKING CHANGE:** with this release any use of the
`storage::internal::ResumableUploadResponse` type requires changes. Applications
should have little need for this type, outside mocks, so the changes should not
affect production code.

Nevertheless, we apologize for the inconvenience, and while we would have
preferred to avoid breaking changes, it was inevitable to introduce some
breaking changes to fix a data loss bug (see [#7835]).

If you are affected by this change, you will need to change your tests following
this guide. Any place where you return a `ResumableUploadResponse` needs to
change from:

```cc
storage::internal::ResumableUploadResource{
  /*upload_session_url=*/std::string{"typically-unused"},
  /*last_committed_byte=*/std::uint64_t{value},
  /*payload=*/absl::nullopt, // or some gcs::ObjectMetadata value
  /*upload_state=*/ResumableUploadResponse::kInProgress, // or kDone
  /*annotations=*/std::string{"typically-unused"}
}
```

to:

```cc
storage::internal::ResumableUploadResource{
  /*upload_session_url=*/std::string{"typically-unused"},
  /*upload_state=*/ResumableUploadResponse::kInProgress, // or kDone
  /*committed_size=*/value + 1, // or absl::nullopt
  /*payload=*/absl::nullopt, // or some gcs::ObjectMetadata value
  /*annotations=*/std::string{"typically-unused"}
}
```

That is, you need to re-order the fields **and** increase the `value` to reflect
the number of committed bytes vs. the index in the last committed byte.

Changing the order of the fields was intentional. It will create a build
failure, which is easier to detect and repair than a run-time error in
your tests.

[#7835]: https://github.com/googleapis/google-cloud-cpp/issues/7835

**OTHER CHANGES**:

* fix(storage): missing options in UpdateObject() ([#8193](https://github.com/googleapis/google-cloud-cpp/pull/8193))
* fix(storage): missing kms key option in CopyObject() ([#8188](https://github.com/googleapis/google-cloud-cpp/pull/8188))
* fix(storage): missing option for CopyObject() ([#8171](https://github.com/googleapis/google-cloud-cpp/pull/8171))
* fix(storage): add missing options for PatchObject() ([#8137](https://github.com/googleapis/google-cloud-cpp/pull/8137))
* feat(storage): per-upload buffer size configuration ([#8096](https://github.com/googleapis/google-cloud-cpp/pull/8096))
* fix(storage): improve JSON validation ([#8033](https://github.com/googleapis/google-cloud-cpp/pull/8033))
* fix(storage)!: handle missing range header in uploads ([#7877](https://github.com/googleapis/google-cloud-cpp/pull/7877))
* cleanup(storage)!: uploads track committed_size ([#7868](https://github.com/googleapis/google-cloud-cpp/pull/7868))

### [Common Libraries](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/README.md)

* fix(generator): fix options handling in SetIamPolicy() OCC loop ([#8203](https://github.com/googleapis/google-cloud-cpp/pull/8203))
* feat(generator): `*Client` with bidir streaming RPCs ([#8187](https://github.com/googleapis/google-cloud-cpp/pull/8187))
* feat(generator): merge connection options into client options ([#8158](https://github.com/googleapis/google-cloud-cpp/pull/8158))
* feat(generator): better metadata decorators for bidir streaming RPCs ([#8077](https://github.com/googleapis/google-cloud-cpp/pull/8077))
* feat(generator): connection respects per call policies ([#8013](https://github.com/googleapis/google-cloud-cpp/pull/8013))
* feat(pubsublite): add TopicStats service ([#7966](https://github.com/googleapis/google-cloud-cpp/pull/7966))
* fix(generator): bidir streaming RPCs improvements ([#7937](https://github.com/googleapis/google-cloud-cpp/pull/7937))
* feat(generator): support bidir streams in Connection ([#7933](https://github.com/googleapis/google-cloud-cpp/pull/7933))
* fix(common): polling policy clones initial state ([#7858](https://github.com/googleapis/google-cloud-cpp/pull/7858))
* fix: add mock library aliases ([#7844](https://github.com/googleapis/google-cloud-cpp/pull/7844))

### New Libraries

We are introducing client libraries for many (>50) GCP services. While we do not
anticipate any API changes to these libraries before declaring them GA, we are
releasing them early in case they elicit some feedback that requires changes.

<details>
<summary> Expand to see the full list of new libraries...</summary>
<br>

* [Access Approval](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/accessapproval/README.md)
* [Access Context Manager](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/accesscontextmanager/README.md)
* [Anthos GKE API](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/gkehub/README.md)
* [API Gateway](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/apigateway/README.md)
* [App Engine Admin API](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/appengine/README.md)
* [Artifact Registry](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/artifactregistry/README.md)
* [Assured Workloads](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/assuredworkloads/README.md)
* [AutoML](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/automl/README.md)
* [Binary Authorization](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/binaryauthorization/README.md)
* [Certificate Authority Service](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/privateca/README.md)
* [Channel Services](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/channel/README.md)
* [Cloud Asset Inventory](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/asset/README.md)
* [Cloud Billing](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/billing/README.md)
* [Cloud Build](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/cloudbuild/README.md)
* [Cloud Composer](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/composer/README.md)
* [Cloud Data Loss Prevention (DLP)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/dlp/README.md)
* [Cloud Debugger](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/debugger/README.md)
* [Cloud Functions](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/functions/README.md)
* [Cloud Intrusion Detection System (IDS)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/ids/README.md)
* [Cloud IoT](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iot/README.md)
* [Cloud Key Management Service (KMS)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/kms/README.md)
* [Cloud Scheduler](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/scheduler/README.md)
* [Cloud Shell](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/shell/README.md)
* [Cloud TPU](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/tpu/README.md)
* [Cloud Trace](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/trace/README.md)
* [Cloud Translation](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/translate/README.md)
* [Cloud Vision](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/vision/README.md)
* [Compute Engine OS Config](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/osconfig/README.md)
* [Compute Engine OS Login](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/oslogin/README.md)
* [Connectivity Tests](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/networkmanagement/README.md)
* [Container Analysis](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/containeranalysis/README.md)
* [Database Migration Service (DMS)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/datamigration/README.md)
* [Eventarc](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/eventarc/README.md)
* [Filestore](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/filestore/README.md)
* [Game Servers](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/gameservices/README.md)
* [Google Kubernetes Engine (GKE)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/container/README.md)
* [Identity-Aware Proxy (IAP)](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iap/README.md)
* [Memorystore for Memcached](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/memcache/README.md)
* [Memorystore for Redis](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/redis/README.md)
* [Migrate for Compute Engine](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/vmmigration/README.md)
* [Organization Policy Service](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/orgpolicy/README.md)
* [Policy Troubleshooter](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/policytroubleshooter/README.md)
* [Recommender](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/recommender/README.md)
* [Resource Manager](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/resourcemanager/README.md)
* [Retail](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/retail/README.md)
* [Security Command Center](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/securitycenter/README.md)
* [Serverless VPC Access](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/vpcaccess/README.md)
* [Service Control](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/servicecontrol/README.md)
* [Service Directory](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/servicedirectory/README.md)
* [Service Management](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/servicemanagement/README.md)
* [Service Usage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/serviceusage/README.md)
* [Storage Transfer Service](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storagetransfer/README.md)
* [Talent Solution](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/talent/README.md)
* [Text-to-Speech](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/texttospeech/README.md)
* [Vertex AI Workbench](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/notebooks/README.md)
* [Video Intelligence API](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/videointelligence/README.md)
* [Web Risk](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/webrisk/README.md)
* [Web Security Scanner](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/websecurityscanner/README.md)
* [Workflows](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/workflows/README.md)
</details>

## v1.35.0 - 2022-01

### New GA Libraries

The following libraries are now considered stable and complete:

* [Secret Manager API](google/cloud/secretmanager/README.md) [[quickstart]](google/cloud/secretmanager/README.md)
* [Cloud Tasks API](google/cloud/tasks/README.md) [[quickstart]](google/cloud/tasks/README.md)

### [BigQuery](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigquery/README.md)

* feat(generator): add options support to generated clients ([#7683](https://github.com/googleapis/google-cloud-cpp/pull/7683))

### [Bigtable](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigtable/README.md)

* feat(bigtable): generate admin APIs ([#7700](https://github.com/googleapis/google-cloud-cpp/pull/7700))

### [IAM](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iam/README.md)

* feat(generator): add options support to generated clients ([#7683](https://github.com/googleapis/google-cloud-cpp/pull/7683))

### [Pub/Sub](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/pubsub/README.md)

* fix(pubsub): Change AsyncReadWriteStreamAuth to be usable with unique_ptr ([#7692](https://github.com/googleapis/google-cloud-cpp/pull/7692))

### [Spanner](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/README.md)

* fix(spanner): switch order of ListBackupOperations() filter conjuncts ([#7746](https://github.com/googleapis/google-cloud-cpp/pull/7746))
* feat(spanner): add per-operation options to Commit() and Rollback() ([#7714](https://github.com/googleapis/google-cloud-cpp/pull/7714))
* feat(spanner): spanner::Client construction from Options ([#7706](https://github.com/googleapis/google-cloud-cpp/pull/7706))
* feat(generator): add options support to generated clients ([#7683](https://github.com/googleapis/google-cloud-cpp/pull/7683))

### [Storage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storage/README.md)

* feat(GCS+gRPC): implement parser for BucketMetadata components ([#7766](https://github.com/googleapis/google-cloud-cpp/pull/7766))
* feat(GCS+gRPC): implement BucketBilling parser ([#7765](https://github.com/googleapis/google-cloud-cpp/pull/7765))
* feat(GCS+gRPC): implement BucketAccessControl parser ([#7763](https://github.com/googleapis/google-cloud-cpp/pull/7763))
* feat(storage): capture metadata info in downloads ([#7694](https://github.com/googleapis/google-cloud-cpp/pull/7694))
* fix: missing dependency for WIN32 ([#7718](https://github.com/googleapis/google-cloud-cpp/pull/7718))
* fix(storage): more strict parsing for HttpResponse ([#7702](https://github.com/googleapis/google-cloud-cpp/pull/7702))

### [Common Libraries](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/README.md)

* Backoff policies are now cloned from their initial state, instead of their
  current state. Any accumulated delay will be reset to its initial value in the
  clone. The previous behavior was a bug, and thus it has been fixed. ([#7696](https://github.com/googleapis/google-cloud-cpp/pull/7696))
* fix: extremely rare race conditions in retry loop ([#7789](https://github.com/googleapis/google-cloud-cpp/pull/7789))
* feat(common): include the request/response type name in the RPC log ([#7782](https://github.com/googleapis/google-cloud-cpp/pull/7782))
* fix(common): revamp the async polling loop ([#7762](https://github.com/googleapis/google-cloud-cpp/pull/7762))
* fix: missing dependency for WIN32 ([#7718](https://github.com/googleapis/google-cloud-cpp/pull/7718))
* doc(common): add a note about AsyncGrpcOperation and OptionsSpan ([#7682](https://github.com/googleapis/google-cloud-cpp/pull/7682))
* feat(common): add support for call-tree-specific options ([#7669](https://github.com/googleapis/google-cloud-cpp/pull/7669))

## v1.34.0 - 2021-12

### [BigQuery](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigquery/README.md) [IAM](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iam/README.md)

Generally we improved the quality of the generated documentation, including:

* feat(generator): document details about `*Client` ([#7673](https://github.com/googleapis/google-cloud-cpp/pull/7673))
* feat(generator): link each method input and output types ([#7665](https://github.com/googleapis/google-cloud-cpp/pull/7665))

### [Bigtable](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigtable/README.md)

* feat(bigtableadmin): add multi-cluster routing for specific clusters ([#7636](https://github.com/googleapis/google-cloud-cpp/pull/7636))

### [Spanner](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/README.md)

* feat(generator): document details about `*Client` ([#7673](https://github.com/googleapis/google-cloud-cpp/pull/7673))
* feat(generator): link each method input and output types ([#7665](https://github.com/googleapis/google-cloud-cpp/pull/7665))
* fix(spanner): GetSingularRow accepts by value ([#7589](https://github.com/googleapis/google-cloud-cpp/pull/7589))
* fix(spanner): avoid use-after-move bugs ([#7588](https://github.com/googleapis/google-cloud-cpp/pull/7588))
* docs(spanner): no doxygen for admin_internal ([#7552](https://github.com/googleapis/google-cloud-cpp/pull/7552))

### [Storage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storage/README.md)

* feat(storage): better debugging for session rewinds ([#7662](https://github.com/googleapis/google-cloud-cpp/pull/7662))
* feat(storage): separate option for download timeouts ([#7655](https://github.com/googleapis/google-cloud-cpp/pull/7655))
* feat: GCS parse "error info" from JSON when available ([#7654](https://github.com/googleapis/google-cloud-cpp/pull/7654))
* doc(storage): describe connection pool ([#7637](https://github.com/googleapis/google-cloud-cpp/pull/7637))
* feat(storage): `storage::Client` is a value type ([#7634](https://github.com/googleapis/google-cloud-cpp/pull/7634))
* feat(storage): default payload format for CreateNotification ([#7633](https://github.com/googleapis/google-cloud-cpp/pull/7633))
* feat(generator): more stable googleapis proto links ([#7606](https://github.com/googleapis/google-cloud-cpp/pull/7606))
* feat(GCS+gRPC): endpoint overrides with secure credentials ([#7572](https://github.com/googleapis/google-cloud-cpp/pull/7572))
* feat(GCS+gRPC): synthetic object metadata links ([#7563](https://github.com/googleapis/google-cloud-cpp/pull/7563))
* fix(GCS+gRPC): correct format Object ids ([#7559](https://github.com/googleapis/google-cloud-cpp/pull/7559))

### [Common Libraries](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/README.md)

* feat: expose and log ErrorInfo if present  ([#7640](https://github.com/googleapis/google-cloud-cpp/pull/7640))
* feat(common): add tracing for the LRO polling loop ([#7615](https://github.com/googleapis/google-cloud-cpp/pull/7615))
* feat(common): add internal-only payload support to Status ([#7603](https://github.com/googleapis/google-cloud-cpp/pull/7603))

### New Libraries

[Cloud Tasks]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/tasks/README.md
[Secret Manager]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/secretmanager/README.md
[Pub/Sub Lite]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/pubsublite/README.md

We are introducing client libraries for [Cloud Tasks], [Secret Manager],
and [Pub/Sub Lite]. These libraries are still under development.

## v1.33.0 - 2021-11

> :warning: In this release we have stopped testing with Ubuntu:16.04 as this
> distribution is no longer supported by Google Cloud. We will gladly consider,
> but do not commit to accepting, patches to fix build problems on the platform.

**ATTENTION**: Our Doxygen documentation (e.g. [Storage
docs][storage-dox-link]) was incorrectly showing the versioned inline namespace
name for our symbols (it was `v1`), implicitly suggesting that users should
spell this inline namespace in their own code. This has been corrected. Our
Doxygen documentation no longer shows the versioned inline namespace name;
instead, it shows users how to correctly spell our symbol names without
referencing the versioned inline namespace.

[storage-dox-link]: https://googleapis.dev/cpp/google-cloud-storage/latest/

### [BigQuery](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigquery/README.md)

* feat: create relocatable pkg-config files ([#7481](https://github.com/googleapis/google-cloud-cpp/pull/7481))

### [Bigtable](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigtable/README.md)

* doc: remove inline namespace from doxygen ([#7461](https://github.com/googleapis/google-cloud-cpp/pull/7461))
* fix(bigtable): retry internal errors known to be transient ([#7395](https://github.com/googleapis/google-cloud-cpp/pull/7395))

### Firestore

**BREAKING CHANGE**: The _experimental_ Firestore support library that used to
live in this repo at `google/cloud/firestore` has been removed in favor of the
canonical library at https://firebase.google.com/docs/reference/cpp. For more
info see [#7443](https://github.com/googleapis/google-cloud-cpp/issues/7443).

* cleanup(firestore)!: removes the experimental firestore library ([#7468](https://github.com/googleapis/google-cloud-cpp/pull/7468))

### [IAM](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iam/README.md)

* feat: create relocatable pkg-config files ([#7481](https://github.com/googleapis/google-cloud-cpp/pull/7481))
* fix(generator): add doxygen return comment for StreamRanges ([#7419](https://github.com/googleapis/google-cloud-cpp/pull/7419))

### [Pub/Sub](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/pubsub/README.md)

* feat: create relocatable pkg-config files ([#7481](https://github.com/googleapis/google-cloud-cpp/pull/7481))
* doc: remove inline namespace from doxygen ([#7461](https://github.com/googleapis/google-cloud-cpp/pull/7461))
* feat(pubsub): implement GUAC
  ([#7449](https://github.com/googleapis/google-cloud-cpp/pull/7449))
  ([#7440](https://github.com/googleapis/google-cloud-cpp/pull/7440))
  ([#7436](https://github.com/googleapis/google-cloud-cpp/pull/7436))
  ([#7432](https://github.com/googleapis/google-cloud-cpp/pull/7432))
  ([#7428](https://github.com/googleapis/google-cloud-cpp/pull/7428))

### [Spanner](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/README.md)

* feat: create relocatable pkg-config files ([#7481](https://github.com/googleapis/google-cloud-cpp/pull/7481))
* doc: remove inline namespace from doxygen ([#7461](https://github.com/googleapis/google-cloud-cpp/pull/7461))
* fix(generator): add doxygen return comment for StreamRanges ([#7419](https://github.com/googleapis/google-cloud-cpp/pull/7419))

### [Storage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storage/README.md)

* fix(storage): respect ContentType in Client::UploadFile ([#7521](https://github.com/googleapis/google-cloud-cpp/pull/7521))
* doc: remove inline namespace from doxygen ([#7461](https://github.com/googleapis/google-cloud-cpp/pull/7461))
* fix(storage): prevent crashes on double Close() ([#7390](https://github.com/googleapis/google-cloud-cpp/pull/7390))
* feat(storage): add bucket attributes for RPO ([#7384](https://github.com/googleapis/google-cloud-cpp/pull/7384))
* doc(storage): 'unspecified' value for PAP is deprecated ([#7377](https://github.com/googleapis/google-cloud-cpp/pull/7377))
* doc(storage): label `ClientOptions` as deprecated ([#7511](https://github.com/googleapis/google-cloud-cpp/pull/7511))

### [Common Libraries](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/README.md)

* fix(common): fewer crashes with dynamic loading ([#7512](https://github.com/googleapis/google-cloud-cpp/pull/7512))
* feat: create relocatable pkg-config files ([#7481](https://github.com/googleapis/google-cloud-cpp/pull/7481))
* fix(common): resume sending "v" in "gccl" component of API header ([#7473](https://github.com/googleapis/google-cloud-cpp/pull/7473))
* doc: remove inline namespace from doxygen ([#7461](https://github.com/googleapis/google-cloud-cpp/pull/7461))
* feat(common): adds begin/end inline namespace macros ([#7456](https://github.com/googleapis/google-cloud-cpp/pull/7456))
* feat(common): GUAC for async read-write streams ([#7442](https://github.com/googleapis/google-cloud-cpp/pull/7442))

## v1.32.1 - 2021-10

### [Pub/Sub](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/pubsub/README.md)

* fix(pubsub): respect non-default values in `pubsub::ConnectionOptions` ([#7406](https://github.com/googleapis/google-cloud-cpp/pull/7406))

## v1.32.0 - 2021-10

### [Bigtable](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigtable/README.md)

* docs(bigtable): add documentation for no channel refreshes ([#7373](https://github.com/googleapis/google-cloud-cpp/pull/7373))

### [IAM](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iam/README.md)

* feat(generator): generate a SetIamPolicy() overload with an OCC loop ([#7276](https://github.com/googleapis/google-cloud-cpp/pull/7276))
* doc(iam): use SetIamPolicy() read-modify-write loop in sample ([#7288](https://github.com/googleapis/google-cloud-cpp/pull/7288))

### [Pub/Sub](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/pubsub/README.md)

* feat(pubsub): use `google::cloud::Options` to configure the library.
  `pubsub::PublisherOptions`, `pubsub::SubscriberOptions`, and related
  functions are deprecated. We have not set a date to remove them, but if
  we ever do we plan to give you one year's notice.
* fix(pubsub): avoid deadlocks in publish flow control ([#7313](https://github.com/googleapis/google-cloud-cpp/pull/7313))
* fix(pubsub): dont std::move PublisherOptions that we are still using ([#7270](https://github.com/googleapis/google-cloud-cpp/pull/7270))

### [Storage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storage/README.md)

**BREAKING CHANGES:**

* We have removed the code generated from `storage/v1` protos. These protos were
  not GA themselves, and the underlying service will be disabled in the near
  future. Furthermore, the library that used them (the GCS+gRPC plugin) is
  clearly labeled `experimental`.  Regardless, we apologize if this causes you
  inconvenience or additional work.

**OTHER CHANGES**:

* fix(storage): eliminate dangling function references ([#7366](https://github.com/googleapis/google-cloud-cpp/pull/7366))
* doc: add gRPC steps to storage quickstart ([#7358](https://github.com/googleapis/google-cloud-cpp/pull/7358))
* fix(storage): restore deprecation warnings on MSVC ([#7325](https://github.com/googleapis/google-cloud-cpp/pull/7325))
* fix(storage): build with MSVC+x86 ([#7323](https://github.com/googleapis/google-cloud-cpp/pull/7323))
* feat(GCS+gRPC): reduce data copying in downloads ([#7303](https://github.com/googleapis/google-cloud-cpp/pull/7303))
* feat(GCS+gRPC): support timeouts for all requests ([#7299](https://github.com/googleapis/google-cloud-cpp/pull/7299))
* feat(storage): support timeouts for all requests ([#7295](https://github.com/googleapis/google-cloud-cpp/pull/7295))
* feat(storage): use generic parameters when resuming uploads ([#7292](https://github.com/googleapis/google-cloud-cpp/pull/7292))
* feat(GCS+gRPC): implement standard parameters ([#7272](https://github.com/googleapis/google-cloud-cpp/pull/7272))
* fix(storage): use CA info options in credentials ([#7261](https://github.com/googleapis/google-cloud-cpp/pull/7261))
* doc(storage): handle kNotFound in example ([#7252](https://github.com/googleapis/google-cloud-cpp/pull/7252))

### [Spanner](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/README.md)

* feat(generator): generate a SetIamPolicy() overload with an OCC loop ([#7276](https://github.com/googleapis/google-cloud-cpp/pull/7276))
* feat(spanner): add factory functions for instance/database/backup ([#7321](https://github.com/googleapis/google-cloud-cpp/pull/7321))
* doc(spanner): convert Spanner samples to use the generated admin APIs ([#7311](https://github.com/googleapis/google-cloud-cpp/pull/7311))
* feat(spanner): add generated admin APIs ([#7285](https://github.com/googleapis/google-cloud-cpp/pull/7285))

### [Common Libraries](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/README.md)

* fix: substitute build metadata (git commit) in bazel builds ([#7378](https://github.com/googleapis/google-cloud-cpp/pull/7378))
* feat(common): add ability to supply a user-run CQ to gRPC options ([#7354](https://github.com/googleapis/google-cloud-cpp/pull/7354))

## v1.31.1 - 2021-09

### [Storage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storage/README.md)

* fix(storage): use CA info options in credentials ([#7263](https://github.com/googleapis/google-cloud-cpp/pull/7263))

## v1.31.0 - 2021-09

### [BigQuery](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigquery/README.md)

* feat(generator): add default tracing components and options ([#7219](https://github.com/googleapis/google-cloud-cpp/pull/7219))

### [Bigtable](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigtable/README.md)

* feat(bigtable): add Make(Data,Admin,InstanceAdmin)Client methods that take Options ([#7226](https://github.com/googleapis/google-cloud-cpp/pull/7226))

### [IAM](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/iam/README.md)

* feat(generator): add default tracing components and options ([#7219](https://github.com/googleapis/google-cloud-cpp/pull/7219))

### [Pub/Sub](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/pubsub/README.md)

* feat(pubsub): support Topic retention duration ([#7196](https://github.com/googleapis/google-cloud-cpp/pull/7196))

### [Storage](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storage/README.md)

* fix: release CurlRequest handles on error ([#7244](https://github.com/googleapis/google-cloud-cpp/pull/7244))
* feat(storage): add additional examples ([#7224](https://github.com/googleapis/google-cloud-cpp/pull/7224))
* feat(storage): aggregate upload benchmark ([#7190](https://github.com/googleapis/google-cloud-cpp/pull/7190))
* fix(GCS+gRPC): quickstart build with Bazel+Windows ([#7147](https://github.com/googleapis/google-cloud-cpp/pull/7147))
* fix(storage): release handles for oauth2 refresh ([#7136](https://github.com/googleapis/google-cloud-cpp/pull/7136))
* fix(GCS+gRPC): return status on stream closure ([#7128](https://github.com/googleapis/google-cloud-cpp/pull/7128))
* feat(storage): make benchmark more robust on stalls ([#7113](https://github.com/googleapis/google-cloud-cpp/pull/7113))
* feat(GCS+gRPC): timeout downloads in benchmark ([#7112](https://github.com/googleapis/google-cloud-cpp/pull/7112))
* feat(GCS+gRPC): initial support for download timeouts ([#7108](https://github.com/googleapis/google-cloud-cpp/pull/7108))

### [Spanner](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/README.md)

* feat(spanner): support for JSON as a column type ([#7212](https://github.com/googleapis/google-cloud-cpp/pull/7212))
* feat(spanner): tagging support ([#7154](https://github.com/googleapis/google-cloud-cpp/pull/7154))
* feat(spanner): send session-refresh request at low priority ([#7140](https://github.com/googleapis/google-cloud-cpp/pull/7140))
* fix(spanner): fix Client::OverlayQueryOptions() to merge correctly ([#7118](https://github.com/googleapis/google-cloud-cpp/pull/7118))

### [Common Libraries](https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/README.md)

**BREAKING CHANGES**:

* `google::cloud::StatusOr<T>` had an accessor that returned an lvalue
  reference to non-const `Status`, this allowed callers to modify the contained
  `Status` and break invariants of the `StatusOr` class. This function was
  removed. If your code previously relied on `sor.status() = new_status` you
  should change it to `sor = new_status`. ([#7150](https://github.com/googleapis/google-cloud-cpp/pull/7150))

**OTHER CHANGES**:

* feat(common): add GrpcChannelArgumentsNativeOption ([#7194](https://github.com/googleapis/google-cloud-cpp/pull/7194))
* feat(common): capture thread that creates log record ([#7119](https://github.com/googleapis/google-cloud-cpp/pull/7119))

## v1.30.1 - 2021-08

### BigQuery

* Support both google-cloud-cpp::bigquery and deprecated google-cloud-cpp::experimental-bigquery targets.

### IAM

* Support both google-cloud-cpp::iam and deprecated google-cloud-cpp::experimental-iam targets.

## v1.30.0 - 2021-08

### New GA Libraries
* *BigQuery Storage* -- The [BigQuery Storage library](https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/bigquery)
  is now GA.
* *IAM* -- The [IAM library](https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/iam) is now GA.

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

**OTHER CHANGES**:
* The `MutationBatcher`'s default setting for max mutations per batch was reduced
  from 100k to 1k. The new value achieves better throughput and avoids errors
  from exhausting the server. ([#7095][https://github.com/googleapis/google-cloud-cpp/pull/7095])

### Storage:

**BREAKING CHANGES**:
* The usage of `google::cloud::internal::ObjectReadSource` has changed. If your
  tests mock how the library uses this class you may need to update your tests.
  We updated the [mocking examples][storage-mocking-link] to guide you in
  changing the tests.

[storage-mocking-link]: https://googleapis.dev/cpp/google-cloud-storage/latest/storage-mocking.html

**OTHER CHANGES**:
* feat(storage): discard handles after an error (#7088)
* feat(storage): release download handles sooner (#7085)
* feat(storage): support setting HTTP version (#7077)
* fix(storage): cleanup interrupted downloads (#7064)
* fix(storage): avoid crashes after move (#7045)
* feat(GCS+gRPC): upload checksums with last chunk (#7031)
* fix(storage): use hash values in InsertObject() (#7025)
* feat(storage): capture peer address for REST (#6994)
* feat(GCS+gRPC): option to configure plugin (#6991)
* feat(GCS+gRPC): checksums on ObjectInsert (#6967)
* fix(GCS+gRPC): no hashes in Object resource (#6963)

### Common Libraries:

* feat(common): always clog GCP_LOG(FATAL) messages (#7087)
* feat(common): make GCP_LOG(FATAL) terminate execution (#7058)

### Other:

**We have removed the `super/` directory:** `google-cloud-cpp` remains usable in
a super build for a larger project, but we do not believe these files add enough
value for the additional complexity. If you prefer to build all the dependencies
from source using CMake, we recommend you use a package manager, such as
[vcpkg][vcpkg-github].

**We have dropped support for Clang < 6.0:** to support the latest Google Cloud
services we need a version of Protobuf that can compile all the `.proto` files
in https://github.com/googleapis/googleapis. At this time this requires
Protobuf >= 3.15, and these versions of Protobuf do not support older versions
of the Clang compiler.

[vcpkg-github]: https://github.com/microsoft/vcpkg

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
