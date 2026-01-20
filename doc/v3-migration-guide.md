# `google-cloud-cpp` v3 Migration Guide

This document is intended for users of previous major versions (v1.x.y, v2.x.y)
of the `google-cloud-cpp` SDK who are moving to a release on the v3.x.y series.

While this repository does not strictly follow semver, it does use the major
version number to indicate large breaking changes. We strive to balance the
frequency in which we introduce breaking changes with improvements to the SDK.
Since our most recent major version increment, about 3 years ago, we have added
new API surfaces that supersede the previous deprecated types and functions. As
part of the v3 release series, we are decommissioning those deprecated API
surfaces. This guide serves a central location to document how to migrate from
the decommissioned API surfaces to their replacements.

## C++17

Depending on your build system of choice, you should set the appropriate flag
for your compiler if it does not already default to `--std=c++17` or higher.

## Bazel Central Registry

Bazel is moving away from WORKSPACE file support to using modules from the Bazel
Central Registry. Part of the v3.x.y release series includes supporting the new
[google-cloud-cpp](https://registry.bazel.build/modules/google_cloud_cpp) Bazel
module which can be added to your `MODULE.bazel` file as a dependency.

## Dependencies

### Previously Optional Dependencies that are now Required

- `libcurl`
- `nlohmann_json`
- `opentelemetry-cpp`

### Relocated Dependencies

- `crc32c`

## Decommissioned API Surfaces

### Bazel

### CMake

### Common

### Bigquery

<details>
<summary>Removed <code>bigquery/retry_traits.h</code> file</summary>

The library no longer exposes the `google/cloud/bigquery/retry_traits.h` header
file. It only contained internal symbols.

</details>

### Bigtable

<details>
<summary>Removed <code>bigtable::RowReader</code> constructors
</summary>

The `bigtable::RowReader` constructors that accept `DataClient` as an argument
have been removed.

Developers that read rows by directly constructing a `RowReader` object should
instead construct a `Table` object and call `Table::ReadRows(...)`.

For example, code that used to look like this:

**Before:**

```cpp
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/table.h"

// ...

auto client = google::cloud::bigtable::MakeDataClient(
    "my-project", "my-instance", creds);
auto reader = google::cloud::bigtable::RowReader(
    client, "my-table-id", google::cloud::bigtable::RowSet("r1", "r2"),
    google::cloud::bigtable::RowReader::NO_ROWS_LIMIT,
    google::cloud::bigtable::Filter::PassAllFilter(),
    /*...retry and backoff policies...*/);

for (auto& row : reader) {
  if (!row) throw std::move(row).status();
  // ...
}
```

Should be changed to this:

**After:**

```cpp
#include "google/cloud/bigtable/table.h"

// ...

namespace cbt = google::cloud::bigtable;
cbt::Table table(cbt::MakeDataConnection(),
                 cbt::TableResource("my-project", "my-instance", "my-table-id"));

for (auto& row : table.ReadRows(
         cbt::RowSet("r1", "r2"),
         cbt::Filter::PassAllFilter())) {
  if (!row) throw std::move(row).status();
  // ...
}
```

</details>

<details>
  <summary>Removed <code>bigtable::ClientOptions</code>
</summary>
#### `bigtable::ClientOptions`

The deprecated `bigtable::ClientOptions` has been removed. Please use
`google::cloud::Options` instead.

The following table shows the mapping from `bigtable::ClientOptions` methods to
their `google::cloud::Options` equivalents:

| `bigtable::ClientOptions` method    | `google::cloud::Options` equivalent                                                             |
| ----------------------------------- | ----------------------------------------------------------------------------------------------- |
| `(constructor)`                     | `google::cloud::Options{}`                                                                      |
| `set_data_endpoint`                 | `google::cloud::EndpointOption`                                                                 |
| `set_admin_endpoint`                | `google::cloud::EndpointOption`                                                                 |
| `set_connection_pool_name`          | `google::cloud::GrpcChannelArgumentsOption` or`google::cloud::GrpcChannelArgumentsNativeOption` |
| `set_connection_pool_size`          | `google::cloud::GrpcNumChannelsOption`                                                          |
| `SetCredentials`                    | `google::cloud::GrpcCredentialOption`                                                           |
| `set_channel_arguments`             | `google::cloud::GrpcChannelArgumentsNativeOption`                                               |
| `SetCompressionAlgorithm`           | `google::cloud::GrpcChannelArgumentsNativeOption`                                               |
| `SetGrpclbFallbackTimeout`          | `google::cloud::GrpcChannelArgumentsNativeOption`                                               |
| `SetUserAgentPrefix`                | `google::cloud::UserAgentProductsOption` or`google::cloud::GrpcChannelArgumentsNativeOption`    |
| `SetResourceQuota`                  | `google::cloud::GrpcChannelArgumentsNativeOption`                                               |
| `SetMaxReceiveMessageSize`          | `google::cloud::GrpcChannelArgumentsNativeOption`                                               |
| `SetMaxSendMessageSize`             | `google::cloud::GrpcChannelArgumentsNativeOption`                                               |
| `SetLoadBalancingPolicyName`        | `google::cloud::GrpcChannelArgumentsNativeOption`                                               |
| `SetServiceConfigJSON`              | `google::cloud::GrpcChannelArgumentsNativeOption`                                               |
| `SetSslTargetNameOverride`          | `google::cloud::GrpcChannelArgumentsNativeOption`                                               |
| `enable_tracing`, `disable_tracing` | `google::cloud::LoggingComponentsOption`                                                        |
| `tracing_options`                   | `google::cloud::GrpcTracingOptionsOption`                                                       |
| `set_max_conn_refresh_period`       | `bigtable::MaxConnectionRefreshOption`                                                          |
| `set_min_conn_refresh_period`       | `bigtable::MinConnectionRefreshOption`                                                          |
| `set_background_thread_pool_size`   | `google::cloud::GrpcBackgroundThreadPoolSizeOption`                                             |
| `DisableBackgroundThreads`          | `google::cloud::GrpcCompletionQueueOption`                                                      |

Example usage of the replacements can be found below.

**Before:**

```cpp
auto client = bigtable::Client(
    bigtable::ClientOptions().set_connection_pool_size(4));
```

**After:**

```cpp
auto client = bigtable::Client(
    google::cloud::Options{}.set<google::cloud::GrpcNumChannelsOption>(4));
```

#### `bigtable::CreateDefaultDataClient`

The deprecated `bigtable::CreateDefaultDataClient` function has been removed.
Please use `bigtable::MakeDataClient` instead.

**Before:**

```cpp
auto client = bigtable::CreateDefaultDataClient(
    "my-project", "my-instance",
    bigtable::ClientOptions().set_connection_pool_size(4));
```

**After:**

```cpp
auto client = bigtable::MakeDataClient(
    "my-project", "my-instance",
    google::cloud::Options{}.set<google::cloud::GrpcNumChannelsOption>(4));
```

#### `bigtable::CreateDefaultAdminClient`

The deprecated `bigtable::CreateDefaultAdminClient` function has been removed.
Please use `bigtable::MakeAdminClient` instead.

**Before:**

```cpp
auto client = bigtable::CreateDefaultAdminClient(
    "my-project", bigtable::ClientOptions().set_connection_pool_size(4));
```

**After:**

```cpp
auto client = bigtable::MakeAdminClient(
    "my-project",
    google::cloud::Options{}.set<google::cloud::GrpcNumChannelsOption>(4));
```

#### `bigtable::CreateDefaultInstanceAdminClient`

The deprecated `bigtable::CreateDefaultInstanceAdminClient` function has been
removed. Please use `bigtable::MakeInstanceAdminClient` instead.

**Before:**

```cpp
auto client = bigtable::CreateDefaultInstanceAdminClient(
    "my-project", bigtable::ClientOptions().set_connection_pool_size(4));
```

**After:**

```cpp
auto client = bigtable::MakeInstanceAdminClient(
    "my-project",
    google::cloud::Options{}.set<google::cloud::GrpcNumChannelsOption>(4));
```

</details>

<details>

<summary>Removed <code>bigtable::AsyncRowReader<>::NO_ROWS_LIMIT</code>
</summary>

AsyncRowReader::NO_ROWS_LIMIT has been removed. Please use
`google::cloud::bigtable::RowReader::NO_ROWS_LIMIT` instead.

```cpp
// Before
auto limit = google::cloud::bigtable::AsyncRowReader<>::NO_ROWS_LIMIT;

// After
auto limit = google::cloud::bigtable::RowReader::NO_ROWS_LIMIT;
```

</details>

<details>
<summary>Removed Endpoint Options
</summary>

The `bigtable::DataEndpointOption`, `bigtable::AdminEndpointOption`, and
`bigtable::InstanceAdminEndpointOption` classes have been removed. Applications
should use `google::cloud::EndpointOption` instead.

**Before:**

```cpp
auto options = google::cloud::Options{}.set<google::cloud::bigtable::DataEndpointOption>("...");
```

**After:**

```cpp
auto options = google::cloud::Options{}.set<google::cloud::EndpointOption>("...");
```

</details>

<details>
<summary>Removed <code>bigtable::DataClient</code> and related functions</summary>

The `bigtable::DataClient` class and its associated factory functions (e.g.,
`MakeDataClient`) have been removed. Applications should now use
`bigtable::DataConnection` and `bigtable::MakeDataConnection()` instead. For
detailed migration steps and examples, please refer to the official migration
guide:

[Migrating from DataClient to DataConnection](https://docs.cloud.google.com/cpp/docs/reference/bigtable/latest/migrating-from-dataclient)

</details>

<details>
<summary>Removed <code>bigtable::MetadataUpdatePolicy</code></summary>

The `bigtable::MetadataUpdatePolicy` class has been removed. It was only used in
internal legacy files.

</details>

### Pubsub

### Spanner

<details>
<summary>Removed <code>spanner::MakeTestRow</code>
</summary>

The `spanner::MakeTestRow` functions have been removed. Please use
`spanner_mocks::MakeRow` instead.

**Before:**

```cpp
#include "google/cloud/spanner/row.h"

// ...

auto row = google::cloud::spanner::MakeTestRow(
    {{"c0", google::cloud::spanner::Value(42)}});
auto row2 = google::cloud::spanner::MakeTestRow(1, "foo", true);
```

**After:**

```cpp
#include "google/cloud/spanner/mocks/row.h"

// ...

auto row = google::cloud::spanner_mocks::MakeRow(
    {{"c0", google::cloud::spanner::Value(42)}});
auto row2 = google::cloud::spanner_mocks::MakeRow(1, "foo", true);
```

</details>

<details>
<summary>Removed <code>spanner::ClientOptions</code> class</summary>

The `spanner::ClientOptions` class has been removed. Use
`google::cloud::Options` instead to set the following as needed:

- `spanner::QueryOptimizerVersionOption`
- `spanner::QueryOptimizerStatisticsPackageOption`
- `spanner::RequestPriorityOption`
- `spanner::RequestTagOption`

**Before:**

```cpp
#include "google/cloud/spanner/client.h"

// ...

namespace spanner = ::google::cloud::spanner;
auto client_options = spanner::ClientOptions().set_query_options(
    spanner::QueryOptions().set_optimizer_version("1"));

auto client = spanner::Client(connection, client_options);
```

**After:**

```cpp
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/options.h"

// ...

namespace spanner = ::google::cloud::spanner;
auto options = google::cloud::Options{}.set<spanner::QueryOptimizerVersionOption>("1");

auto client = spanner::Client(connection, options);
```

</details>

<details>

<summary>Removed <code>admin/retry_traits.h</code> file</summary>

The library no longer exposes `google/cloud/spanner/admin/retry_traits.h` header
file. It only contained internal symbols.

</details>

<details>
<summary>Removed Admin Clients from <code>spanner</code> namespace</summary>

The `DatabaseAdminClient` and `InstanceAdminClient` classes (and their
associated connection classes and factory functions) have been removed from the
`google::cloud::spanner` namespace. Please use the replacements in
`google::cloud::spanner_admin`.

**Before:**

```cpp
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/instance_admin_client.h"

namespace spanner = ::google::cloud::spanner;

void Function(spanner::DatabaseAdminClient db_admin,
              spanner::InstanceAdminClient in_admin) {
  // ...
}
```

**After:**

```cpp
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"

namespace spanner_admin = ::google::cloud::spanner_admin;

void Function(spanner_admin::DatabaseAdminClient db_admin,
              spanner_admin::InstanceAdminClient in_admin) {
  // ...
}
```

</details>

### Storage

<details>
<summary><code>ClientOptions</code> is removed</summary>

The `ClientOptions` class is no longer available. You should now use
`google::cloud::Options` to configure the `Client`.

**Before:**

```cpp
#include "google/cloud/storage/client.h"

void CreateClient() {
  auto credentials = google::cloud::storage::oauth2::GoogleDefaultCredentials().value();
  auto options = google::cloud::storage::ClientOptions(credentials);
  options.set_project_id("my-project");
  options.set_upload_buffer_size(1024 * 1024);

  google::cloud::storage::Client client(options);
}
```

**After:**

```cpp
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/options.h" // For option structs

void CreateClient() {
  auto credentials = google::cloud::MakeGoogleDefaultCredentials();
  auto client = google::cloud::storage::Client(
      google::cloud::Options{}
          .set<google::cloud::Oauth2CredentialsOption>(credentials)
          .set<google::cloud::storage::ProjectIdOption>("my-project")
          .set<google::cloud::storage::UploadBufferSizeOption>(1024 * 1024));
}
```

Use the following table to map `ClientOptions` setters to
`google::cloud::Options`:

| `ClientOptions` Method                | Replacement Option (`.set<T>(value)`)                   |
| :------------------------------------ | :------------------------------------------------------ |
| `set_credentials(c)`                  | `google::cloud::storage::Oauth2CredentialsOption`       |
| `set_project_id(p)`                   | `google::cloud::storage::ProjectIdOption`               |
| `set_endpoint(url)`                   | `google::cloud::storage::RestEndpointOption`            |
| `set_iam_endpoint(url)`               | `google::cloud::storage::IamEndpointOption`             |
| `SetDownloadBufferSize`               | `google::cloud::storage::DownloadBufferSizeOption`      |
| `SetUploadBufferSizee`                | `google::cloud::storage::UploadBufferSizeOption`        |
| `set_maximum_simple_upload_size(s)`   | `google::cloud::storage::MaximumSimpleUploadSizeOption` |
| `set_enable_http_tracing(true)`       | `google::cloud::LoggingComponentsOption`                |
| `set_enable_raw_client_tracing(true)` | `google::cloud::LoggingComponentsOption`                |

**Example for Tracing:**

```cpp
// Before
options.set_enable_http_tracing(true);

// After
auto opts = Options{}.lookup<LoggingComponentsOption>().insert("raw-client");
```

</details>

<details>
<summary><code>ChannelOptions</code> is removed</summary>

The `ChannelOptions` class is no longer available. You should now use
`google::cloud::Options` to configure the transport channel.

**Before:**

```cpp
#include "google/cloud/storage/grpc_plugin.h"

void CreateClient() {
  auto options = google::cloud::storage::ChannelOptions()
      .set_ssl_root_path("path/to/roots.pem");

  auto client = google::cloud::storage::MakeGrpcClient(
      google::cloud::storage::ClientOptions(), options);
}
```

**After:**

```cpp
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/common_options.h"

void CreateClient() {
  auto client = google::cloud::storage::MakeGrpcClient(
      google::cloud::Options{}.set<google::cloud::CARootsFilePathOption>(
          "path/to/roots.pem"));
}
```

</details>

<details>
<summary>ChannelOptions Mapping</summary>

Use the following table to map `ChannelOptions` setters to
`google::cloud::Options`:

| `ChannelOptions` Method | Replacement Option (`.set<T>(value)`)  |
| :---------------------- | :------------------------------------- |
| `set_ssl_root_path(p)`  | `google::cloud::CARootsFilePathOption` |

</details>

<details>
<summary><code>Client</code> Constructor removal</summary>

The constructor `Client(ClientOptions)` is removed. The default constructor
`Client()` generally uses default options and default credentials. To customize,
use `Client(Options)`.

**Before:**

```cpp
#include "google/cloud/storage/client.h"

void CreateClient() {
  auto credentials = google::cloud::storage::oauth2::GoogleDefaultCredentials().value();
  auto options = google::cloud::storage::ClientOptions(credentials);
  auto client = google::cloud::storage::Client(options);
}
```

**After:**

```cpp
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/options.h"

void CreateClient() {
  auto credentials = google::cloud::MakeGoogleDefaultCredentials();
  auto client = google::cloud::storage::Client(
      google::cloud::Options{}.set<google::cloud::storage::Oauth2CredentialsOption>(credentials));
}
```

</details>

<details>
<summary>Removed <code>Client(Connection, NoDecorations)</code> constructor</summary>

The `Client` constructor that accepted a `StorageConnection` and the
`NoDecorations` tag has been removed. This was intended only for test code.

**Before:**

```cpp
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/mock_storage_connection.h"

void TestClient() {
  auto mock = std::make_shared<google::cloud::storage::testing::MockStorageConnection>();
  // ...
  auto client = google::cloud::storage::Client(
      mock, google::cloud::storage::Client::NoDecorations{});
}
```

**After:**

```cpp
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/mock_storage_connection.h"

void TestClient() {
  auto mock = std::make_shared<google::cloud::storage::testing::MockStorageConnection>();
  // ...
  auto client = google::cloud::storage::internal::ClientImplDetails::CreateWithoutDecorations(mock);
}
```

</details>

<details>
<summary>Removed <code>Client::raw_client()</code></summary>

The `Client::raw_client()` method has been removed. This was intended only for
internal use or testing. If you need access to the underlying connection for
testing purposes, use `google::cloud::storage::internal::ClientImplDetails`.

**Before:**

```cpp
#include "google/cloud/storage/client.h"

void UseRawClient(google::cloud::storage::Client client) {
  auto connection = client.raw_client();
}
```

**After:**

```cpp
#include "google/cloud/storage/client.h"

void UseRawClient(google::cloud::storage::Client client) {
  auto connection =
      google::cloud::storage::internal::ClientImplDetails::GetConnection(client);
}
```

</details>

<details>
<summary>Removed deprecated <code>Oauth2CredentialsOption</code></summary>

The `google::cloud::UnifiedCredentialsOption` and the unified credentials API
documented at
https://docs.cloud.google.com/cpp/docs/reference/common/latest/group__guac
should be used instead.

**Before:**

```cpp
#include "google/cloud/options.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"

namespace gc = ::google::cloud;
namespace gcs = ::google::cloud::storage;
namespace oauth2 = ::google::cloud::storage::oauth2;

auto options = gc::Options{}
    .set<gcs::Oauth2CredentialsOption>(oauth2::CreateAnonymousCredentials());
auto client = gcs::Client(options);
```

**After:**

```cpp
#include "google/cloud/options.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/credentials.h"

namespace gc = ::google::cloud;
namespace gcs = ::google::cloud::storage;

auto options = gc::Options{}
    .set<gc::UnifiedCredentialsOption>(gc::MakeInsecureCredentials());
auto client = gcs::Client(options);
```

</details>

<details>
<summary>Removed deprecated <code>CreateServiceAccountCredentialsFromFilePath</code></summary>

The `google::cloud::MakeServiceAccountCredentialsFromFile` factory function and
associated override options `google::cloud::ScopesOption` and
`google::cloud::subjectOption` should be used instead.

**Before:**

```cpp
#include "google/cloud/options.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"

namespace gc = ::google::cloud;
namespace gcs = ::google::cloud::storage;
namespace oauth2 = ::google::cloud::storage::oauth2;

std::set<std::string> scopes = {"scope1", "scope2"};
auto credentials = CreateServiceAccountCredentialsFromFilePath(
    "path-to-file", scopes, "my-subject");

auto options = gc::Options{}
    .set<gcs::Oauth2CredentialsOption>(credentials);
auto client = gcs::Client(options);
```

**After:**

```cpp
#include "google/cloud/options.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/credentials.h"

namespace gc = ::google::cloud;
namespace gcs = ::google::cloud::storage;

auto options = gc::Options{}
    .set<gc::ScopesOption>(std::vector<std::string>({"scope1", "scope2"}))
    .set<gc::SubjectOption>("my-subject");

options = options.set<gc::UnifiedCredentialsOption>(
    gc::MakeServiceAccountCredentialsFromFile("path-to-file", options));
auto client = gcs::Client(options);
```

</details>

### IAM

<details>

<summary>Removed <code>iam/retry_traits.h</code> file</summary>

</details>
The library no longer exposes `google/cloud/iam/retry_traits.h` header file. It
only contained internal symbols.

</details>
