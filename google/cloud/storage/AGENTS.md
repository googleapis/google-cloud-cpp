# Agent Guidelines for Cloud Storage (`google/cloud/storage/`)

`google/cloud/storage/` is ~98% hand-crafted and builds **two distinct client
libraries** (`//:storage` and `//:storage_grpc`). Separate GAPIC control-plane
and tooling services live in sibling repository directories
(`google/cloud/storagecontrol/`, `storagebatchoperations/`, `storageinsights/`,
`storagetransfer/`).

## Dual Targets and Source Lists

1. **`//:storage`** (`google_cloud_cpp_storage.cmake` ->
   `google_cloud_cpp_storage.bzl`, CMake `google-cloud-cpp::storage`):
   - Synchronous `storage::Client` (`client.h`, `ObjectReadStream`,
     `ObjectWriteStream`) over HTTP/JSON/XML via `libcurl` (`internal/curl/`,
     `internal/rest/`).
   - **Zero gRPC or Protobuf dependencies.** Never introduce `.pb.h` or gRPC
     headers into files owned by `google_cloud_cpp_storage.cmake`.
1. **`//:storage_grpc`** (`google_cloud_cpp_storage_grpc.cmake` ->
   `google_cloud_cpp_storage_grpc.bzl`, CMake
   `-DGOOGLE_CLOUD_CPP_ENABLE=storage_grpc` -> `google-cloud-cpp::storage_grpc`,
   defines `GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC`):
   - Synchronous gRPC plugin `storage::MakeGrpcClient()` (`grpc_plugin.h`,
     `internal/grpc/`, adapting gRPC `StorageStub` to `internal::GenericStub`).
   - **GA Async Client** (`storage::AsyncClient`, `AsyncConnection`,
     `AsyncReader`, `AsyncWriter`, `AsyncRewriter`, and bidi-streaming
     `ObjectDescriptor` in `storage/async/` and `internal/async/`), using
     `absl::Cord`-backed `ReadPayload` / `WritePayload`.
   - Optional gRPC client-side OTel metrics
     (`--//google/cloud/storage:enable_grpc_metrics`,
     `GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS`).

Update the matching `.cmake` file (`google_cloud_cpp_storage.cmake` vs.
`google_cloud_cpp_storage_grpc.cmake`) and regenerate its `.bzl` list whenever
adding or removing files.

## Generated vs. Hand-Crafted Boundary

- **Generated (`DO NOT EDIT`):** Only the 12 low-level gRPC stub files in
  `internal/` (`storage_stub`, `storage_auth_decorator`,
  `storage_logging_decorator`, `storage_metadata_decorator`,
  `storage_round_robin_decorator`, and `storage_tracing_stub` `.{h,cc}`,
  generated from `google/storage/v2/storage.proto`).
- **Hand-Crafted:** Every other file in `storage/`, `storage/async/`,
  `storage/internal/`, `storage/mocks/`, `storage/testing/`, and
  `storage/tests/`.

## Critical Rule: `[ctype = CORD]` on `ChecksummedData`

- `google.storage.v2.ChecksummedData.content` uses `[ctype = CORD]`, which
  generates incompatible C++ APIs across Protobuf `< 4.23` (`std::string` with
  private accessors requiring workarounds) vs. Protobuf `>= 4.23` (`absl::Cord`,
  where `.mutable_content()` does not exist; see
  `doc/ctype-cord-workarounds.md`).
- **Hard Rule:** **Never** call `.content()`, `.mutable_content()`, or
  `.set_content()` directly on `google::storage::v2::ChecksummedData` in library
  or test code. Always use `storage_internal::GetContent()`, `SetContent()`,
  `SetMutableContent()`, and `StealMutableContent()` from
  `google/cloud/storage/internal/grpc/ctype_cord_workaround.h`.

## Storage-Specific Design, Namespaces, and Mocks

- **Sync `storage::Client` Conventions (`doc/adr/`):**
  - Per-request optional parameters are passed as variadic template arguments
    (`storage::IfGenerationMatch(...)`, `storage::UserProject(...)`), not via
    mutable client state.
  - By default, sync GCS retries all operations unless constrained by the retry
    or idempotency policy (`IdempotencyPolicyOption`).
  - Resource metadata classes wrap fields in `std::optional<T>` only for nested
    sub-objects (providing `has_foo()`, `foo()`, `foo_as_optional()`), while
    strings, vectors, integers, and booleans default to empty/`0`/`false`.
- **Mocks (`//:storage_mocks` vs. `//:storage_grpc_mocks`):**
  - **`//:storage_mocks` (`google-cloud-cpp::storage_mocks`) is intentionally
    empty** because sync `storage::internal::StorageConnection` and its
    `*Request` types still reside in `storage::internal`.
  - Public mocks exist in **`//:storage_grpc_mocks`**
    (`google-cloud-cpp::storage_grpc_mocks` in
    `namespace google::cloud::storage_mocks`) for `AsyncClient`
    (`MockAsyncConnection`, `MockAsyncReaderConnection`,
    `MockAsyncWriterConnection`, `MockAsyncObjectDescriptorConnection`,
    `MockAsyncRewriterConnection`).
- **Internal Namespaces:** Legacy sync/REST internals use nested
  `google::cloud::storage::internal`; gRPC and async internals use
  `google::cloud::storage_internal`.

## Running Tests

- Unit tests (REST + gRPC/Async): `bazelisk test`
  `--test_tag_filters=-integration-test //google/cloud/storage/...`
- Emulator integration tests (launches the Python GCS testbench, running gRPC
  tests across `default` and `grpc-metadata` variations):
  `google/cloud/storage/ci/run_integration_tests_emulator_bazel.sh`
