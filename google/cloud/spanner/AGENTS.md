# Agent Guidelines for Cloud Spanner (`google/cloud/spanner/`)

Cloud Spanner is a **hybrid** library combining a hand-crafted data-plane
client, session manager, streaming result reassembler, and SQL value/row type
system with generated admin clients and low-level gRPC stubs.

## Generated vs. Hand-Crafted Boundaries

- **Generated (`DO NOT EDIT`):**
  1. **`spanner/admin/*`:** `DatabaseAdminClient` and `InstanceAdminClient` in
     `namespace google::cloud::spanner_admin` (mocks in `spanner_admin_mocks`),
     generated with **both gRPC and HTTP/REST transcoding**
     (`database_admin_rest_connection.h`, `instance_admin_rest_connection.h`,
     and `admin/internal/*_rest_*`).
  1. **Data-plane gRPC stubs in `internal/`:** The 10 files `spanner_stub`,
     `spanner_auth_decorator`, `spanner_logging_decorator`,
     `spanner_metadata_decorator`, and `spanner_tracing_stub` `.{h,cc}`
     (generated with `experimental_operation_context: true` and **without** a
     generated round-robin decorator because `SessionPool` manages channel
     affinity directly).
- **Hand-Crafted:** Data-plane `spanner::Client`, `internal::ConnectionImpl`,
  `Transaction`, `SessionPool`, `PartialResultSetSource` /
  `PartialResultSetResume` / `MergeChunk`, `Value`, `Row`, `RowStream`,
  `SqlStatement`, `Mutations`, `KeySet`, `route_to_leader.h`,
  `spanner_request_id.h`, `mocks/`, `testing/`, and `integration_tests/`.

## Deprecated Legacy APIs and Test Helpers

- **Legacy Admin Clients:** Top-level `spanner::DatabaseAdminClient`,
  `spanner::InstanceAdminClient`, and
  `internal/{database,instance}_admin_{stub,logging,metadata}.{h,cc}` are
  **deprecated** (`GOOGLE_CLOUD_CPP_SPANNER_ADMIN_API_DEPRECATED` in
  `spanner/version.h`). All active admin work belongs in `spanner/admin/`
  (`namespace google::cloud::spanner_admin`).
- **Mocking `Row`s:** `spanner::MakeTestRow()` is deprecated
  (`GOOGLE_CLOUD_CPP_SPANNER_MAKE_TEST_ROW_DEPRECATED`). Always construct mock
  rows with `spanner_mocks::MakeRow(...)` (`google/cloud/spanner/mocks/row.h`)
  and mock result streams with `spanner_mocks::MockResultSetSource`.

## Session, Streaming, and Transaction Invariants

- **`SessionPool` vs. Multiplexed Sessions (`internal/session_pool.{h,cc}`):**
  - Regular pooled sessions (`BatchCreateSessions`, LIFO reuse, background
    keep-alive refresh) are pinned to specific `SpannerStub` / gRPC channel
    instances for channel affinity.
  - Multiplexed sessions (`EnableMultiplexedSessionOption` /
    `SessionPool::Multiplexed()`) use a single channel-agnostic session
    supporting concurrent transactions and rotated periodically on a background
    timer.
  - Integration tests in `spanner/integration_tests/BUILD.bazel` run
    session-sensitive suites under both modes
    (`GOOGLE_CLOUD_CPP_SPANNER_TESTING_SESSION_MODE={pool,multiplexed}`) against
    `SPANNER_EMULATOR_HOST` or production.
- **Streaming Reassembly & Transaction Reruns:**
  - `PartialResultSetResume`, `PartialResultSetSource`, and `MergeChunk` handle
    `ExecuteStreamingSql` / `StreamingRead` resume tokens and chunked
    `google::protobuf::Value` merging across `PartialResultSet` boundaries into
    `spanner::Row`s (see `internal/README.md` for `Row` tuple parsing).
  - Read-write `Client::Commit(TransactionMutator, ...)` automatically reruns
    the user's mutator callable on `StatusCode::kAborted` (honoring `RetryInfo`
    backoff via `StatusRerunPolicy`) while preserving the underlying
    `Transaction::Impl` session, priority, and tag state.
  - Outgoing RPCs attach leader-routing (`x-goog-spanner-route-to-leader` via
    `internal/route_to_leader.h`) and monotonic request IDs
    (`x-goog-spanner-request-id` via `internal/spanner_request_id.h`).

## Build Targets and Verification

- **Optional Admin REST Target:** Preserve `google_cloud_cpp_spanner_rest`
  (`google_cloud_cpp_spanner_rest.bzl`, CMake `experimental-http-transcoding`)
  and `# Do not sort: grpc++ must come last` in `spanner/BUILD.bazel` when
  updating build files.
- Unit tests: `bazelisk test --test_tag_filters=-integration-test`
  `//google/cloud/spanner/...`
