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

### Pubsub

### Spanner

### Storage
