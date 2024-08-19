# Future Breaking Changes

The next major version of `google-cloud-cpp` (the 3.x series) is expected to
include the following **breaking changes**.

<details>
<summary>Bigtable: mark <code>bigtable::DataClient</code> as <code>final</code>
</summary>
<br>

We will mark `bigtable::DataClient` as `final` and remove the following member
functions:

- `DataClient::Channel()`
- `DataClient::reset()`
- `DataClient::BackgroundThreadsFactory()`

Application developers should not need to interact with this class directly.
Please use `google::cloud::bigtable::MakeDataClient(...)` with the options from
`google::cloud::GrpcOptionList<>` if you need to configure the gRPC channel or
background threads. More details can be found in [#8800].

</details>

<details>
<summary>Bigtable: remove <code>bigtable::RowReader</code> constructors
</summary>
<br>

We will remove `bigtable::RowReader` constructors which accept `DataClient` as
an argument.

Application developers who read rows by directly constructing a `RowReader`
object should instead construct a `Table` object and call `Table::ReadRows(...)`
on it.

For status on this, see [#8854].

</details>

<details>
<summary>BigQuery: remove <code>BigQueryReadReadRowsStreamingUpdater</code> from
<code>google::cloud::bigquery</code> namespace</summary>
<br>

We will remove `BigQueryReadReadRowsStreamingUpdater` from its declaration in
`google/cloud/bigquery/read_connection.h` and from the `google::cloud::bigquery`
namespace. The function continues to exist but in an internal file and
namespace. For status on this see [#8234].

</details>

<details>
<summary>Pubsub: remove legacy admin APIs</summary>
<br>

We will remove the hand-written versions of the Pub/Sub admin APIs. These have
been superseded by versions generated automatically from the service
definitions. The new APIs can be found in the
[`google/cloud/pubsub/admin`](https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/pubsub/admin)
tree and within the `google::cloud::pubsub_admin` namespace. Starting with the
v2.23.0 release, and depending on your compiler settings, using the old
classes/functions may elicit a deprecation warning. See [#12987] for more
details.

</details>

<details>
<summary>Spanner: remove legacy admin APIs</summary>
<br>

We will remove the hand-written versions of the Spanner admin APIs. These have
been superseded by versions generated automatically from the service
definitions. The new APIs can be found in the
[`google/cloud/spanner/admin`](https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/spanner/admin)
tree and within the `google::cloud::spanner_admin` namespace. Starting with the
v1.32.0 release, and depending on your compiler settings, using the old
classes/functions may elicit a deprecation warning. See [#7356] for more
details.

</details>

<details>
<summary>Spanner: remove <code>spanner::MakeTestRow()</code></summary>
<br>

We will remove `spanner::MakeTestRow()`, which has been replaced by
`spanner_mocks::MakeRow()`. Starting with the v1.41.0 release, and depending on
your compiler settings, using `spanner::MakeTestRow()` may elicit a deprecation
warning. See [#9086] for more details.

</details>

<details>
<summary>Storage: remove the `google::cloud::storage::oauth2` namespace</summary>
<br>

We will remove the `google::cloud::storage::oauth2` namespace, and all the
header files in `google/cloud/storage/oauth2/`. The new credential types in
`google/cloud/credentials.h` are work with gRPC-based services and support more
use-cases. To create access tokens use the supporting classes in
`google/cloud/oauth2/`.

</details>

<details>
<summary>Common Libraries: remove <code>v1</code> and <code>gcpcxxV1</code>
backward compatibility aliases</summary>
<br>

We will remove the inline namespace aliases of `v1` and `gcpcxxV1` that are
declared in `google/cloud/version.h`. These aliases exist because we changed the
format of our inline namespace name to include the major, minor, and patch
numbers for each release, and we didn't want to break users. Our Doxygen
documentation was mistakenly recommending that users include the inline
namespace names in their code, but this was also fixed to now recommend against
doing so. Users should generally omit the name of our versioned inline namespace
name because that will tightly couple their code to a specific release, and will
make upgrading to newer versions more difficult. Instead, users should simply
omit the inline namespace name, for example, instead of
~`google::cloud::v1::Status`~ use `google::cloud::Status`. Please update your
code to avoid naming the `v1` and `gcpcxxV1` inline namespace names. For more
info, see [#7463] and [#5976].

</details>

<details>
<summary>Compute: remove deprecated google/cloud/compute/service/service_proto_export.h files
</summary>
<br>

These export files have already been relocated to
protos/google/cloud/compute/service/service_proto_export.h such that they are
nearer to the internal proto files that they provide access to [#14654].

</details>

[#12987]: https://github.com/googleapis/google-cloud-cpp/issues/12987
[#14654]: https://github.com/googleapis/google-cloud-cpp/issues/14654
[#5976]: https://github.com/googleapis/google-cloud-cpp/issues/5976
[#7356]: https://github.com/googleapis/google-cloud-cpp/issues/7356
[#7463]: https://github.com/googleapis/google-cloud-cpp/issues/7463
[#8234]: https://github.com/googleapis/google-cloud-cpp/issues/8234
[#8800]: https://github.com/googleapis/google-cloud-cpp/issues/8800
[#8854]: https://github.com/googleapis/google-cloud-cpp/issues/8854
[#9086]: https://github.com/googleapis/google-cloud-cpp/issues/9086
