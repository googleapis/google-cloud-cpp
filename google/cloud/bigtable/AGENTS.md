# Agent Guidelines for Cloud Bigtable (`google/cloud/bigtable/`)

Cloud Bigtable is a **hybrid** library combining hand-crafted data-plane, SQL,
batching, channel-pool, and OpenTelemetry metrics code with generated admin
clients and low-level gRPC stubs.

## Generated vs. Hand-Crafted Boundaries

- **Generated (`DO NOT EDIT`):**
  - `admin/*` (`BigtableInstanceAdminClient` and `BigtableTableAdminClient` in
    `namespace google::cloud::bigtable_admin`, mocks in `bigtable_admin_mocks`)
    — **except**
    `admin/internal/bigtable_table_admin_connection_impl_bespoke.cc`, which is
    hand-crafted (`WaitForConsistency` / `AsyncWaitForConsistency`, wired via
    `bespoke_methods` in `generator/generator_config.textproto`).
  - Data-plane gRPC stubs in `internal/` (12 files: `bigtable_stub`,
    `bigtable_auth_decorator`, `bigtable_logging_decorator`,
    `bigtable_metadata_decorator`, `bigtable_round_robin_decorator`, and
    `bigtable_tracing_stub` `.{h,cc}`), generated from
    `google/bigtable/v2/bigtable.proto` with
    `experimental_operation_context: true` (injecting
    `bigtable_internal::OperationContext` into stub calls).
- **Hand-Crafted:** All other files in `bigtable/`, `bigtable/internal/`,
  `bigtable/mocks/`, `bigtable/testing/`, `bigtable/tests/`,
  `bigtable/test_proxy/`, `bigtable/benchmarks/`, and `bigtable/examples/`.

## Data-Plane Architecture and Namespaces

- **Three Veneers Over `bigtable::DataConnection`:**
  1. **`bigtable::Table` (`table.h`):** Row-oriented operations (`Apply`,
     `AsyncApply`, `BulkApply`, `AsyncBulkApply`, `ReadRow`,
     `ReadRows` -> `RowReader`, `CheckAndMutateRow`, `ReadModifyWriteRow`,
     `SampleRows`) bound to a table resource and optional `app_profile_id`.
  1. **`bigtable::Client` (`client.h`):** SQL query execution (`PrepareQuery`,
     `AsyncPrepareQuery`, `ExecuteQuery(BoundQuery&&)` -> `RowStream` of
     `QueryRow`), backed by `internal::QueryPlan` for automatic expired-plan
     refresh.
  1. **`bigtable::MutationBatcher` (`mutation_batcher.h`):** Client-side async
     write batching with byte/count flow control.
- **Namespaces:** Data plane uses `bigtable`, `bigtable_mocks`, and
  `bigtable_internal`; admin plane uses `bigtable_admin`,
  `bigtable_admin_mocks`, and `bigtable_admin_internal`.
- **Channel Pools & Metrics:** `DataConnection` supports both static channel
  pools (`bigtable_channel_refresh`, `bigtable_random_two_least_used_decorator`)
  and multi-instance `DynamicChannelPool` + DirectPath probing. Client-side OTel
  metrics are controlled by `GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS`
  (Bazel flag `--//google/cloud/bigtable:enable_metrics=True`).

## Testing, Mocking, and Build Quirks

- **Mocking `ReadRows` on `MockDataConnection`:** In
  `bigtable_mocks::MockDataConnection` (`mocks/mock_data_connection.h`), the
  3-argument and 4-argument `ReadRows(...)` overloads delegate via default
  `ON_CALL` actions to `ReadRowsFull(ReadRowsParams)`. Set test expectations on
  `EXPECT_CALL(*conn, ReadRowsFull(...))` and construct fake readers with
  `bigtable_mocks::MakeRowReader(...)`.
- **`readrowsparser_test.cc` Special Wiring:** `internal/readrowsparser_test.cc`
  depends on `internal/readrowsparser_acceptance_tests.inc`; it has a standalone
  `cc_test` rule in `BUILD.bazel` and is registered *after*
  `export_list_to_bazel` in `CMakeLists.txt`.
- **Running Tests:**
  - Unit tests: `bazelisk test --test_tag_filters=-integration-test`
    `//google/cloud/bigtable/...`
  - Integration tests (`bigtable/tests/`): Parameterized across `default` and
    `dynamic-pool` (`GOOGLE_CLOUD_CPP_BIGTABLE_TESTING_CHANNEL_POOL`) against
    `BIGTABLE_EMULATOR_HOST` / `BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST`.
