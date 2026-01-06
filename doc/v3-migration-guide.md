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
<summary>Removed <code>bigtable::DataClient</code> and related functions</summary>

The `bigtable::DataClient` class and its associated factory functions (e.g.,
`MakeDataClient`) have been removed. Applications should now use
`bigtable::DataConnection` and `bigtable::MakeDataConnection()` instead. For
detailed migration steps and examples, please refer to the official migration
guide:

[Migrating from DataClient to DataConnection](https://docs.cloud.google.com/cpp/docs/reference/bigtable/latest/migrating-from-dataclient)

</details>

### Pubsub

### Spanner

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
<summary><code>Client</code> Constructor</summary>

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
