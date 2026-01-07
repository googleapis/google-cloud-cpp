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

### Pubsub

### Spanner

<details>
<summary>Removed <code>admin/retry_traits.h</code> file</summary>

The library no longer exposes `google/cloud/spanner/admin/retry_traits.h` header
file. It only contained internal symbols.

</details>

### Storage

### IAM

<details>
<summary>Removed <code>iam/retry_traits.h</code> file</summary>

The library no longer exposes `google/cloud/iam/retry_traits.h` header file. It
only contained internal symbols.

</details>
