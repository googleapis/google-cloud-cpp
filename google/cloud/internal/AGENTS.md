# Agent Guidelines for Core Runtime Libraries

This file governs core SDK components across `google/cloud/internal/`, top-level
`google/cloud/*.{h,cc}`, `google/cloud/mocks/`, `google/cloud/universe_domain/`,
`google/cloud/opentelemetry/`, and `google/cloud/testing_util/`.

*(Note: This file lives in `google/cloud/internal/` rather than `google/cloud/`
so directory inheritance does not inject core-maintainer rules into all 160+
service libraries under `google/cloud/<service>/`.)*

## Core Production Sub-Targets and Dependency Walls

`google/cloud/CMakeLists.txt` and `google/cloud/BUILD.bazel` partition files in
`google/cloud/` and `google/cloud/internal/` into five production sub-libraries,
plus `mocks` and `opentelemetry`. Respect their strict dependency boundaries
when placing or modifying files:

1. **`google_cloud_cpp_common`** (`google/cloud/google_cloud_cpp_common.cmake`
   -> `google_cloud_cpp_common.bzl`, Bazel `//:common`):
   - **Zero gRPC or `libcurl` dependencies** (depends only on Abseil,
     OpenSSL/Win32 crypto, and header-only `@opentelemetry-cpp//api`).
   - Owns `Status`, `StatusOr<T>`, `Options`, `Credentials`, `future<T>` /
     `promise<T>`, `StreamRange<T>`, `RetryPolicy`, `BackoffPolicy`,
     `PollingPolicy`, `Idempotency`, `TracingOptions`, `version.h`,
     `internal/throw_delegate.{h,cc}`, base64/SHA256/RFC3339 helpers, and
     `internal/opentelemetry.{h,cc}`.
1. **`google_cloud_cpp_grpc_utils`**
   (`google/cloud/google_cloud_cpp_grpc_utils.cmake` -> `.bzl`, Bazel
   `//:grpc_utils`):
   - Owns all gRPC transport primitives: `CompletionQueue`, `AsyncOperation`,
     `BackgroundThreads`, `GrpcOptions`, `grpc_error_delegate`, unified gRPC
     credentials (`unified_grpc_credentials`, `minimal_iam_credentials_stub`),
     sync/async retry and LRO polling loops, sync/async streaming RPC wrappers,
     `routing_matcher.h`, and `grpc_opentelemetry`.
1. **`google_cloud_cpp_rest_internal`**
   (`google/cloud/google_cloud_cpp_rest_internal.cmake` -> `.bzl`, internal
   target `//google/cloud:google_cloud_cpp_rest_internal`):
   - **Zero Protobuf or gRPC dependencies.** Uses `libcurl`, `nlohmann_json`,
     and OpenSSL.
   - Owns `RestClient`, `CurlRestClient`, `RestRequest`, `RestResponse`,
     `rest_options.h`, `rest_retry_loop.h`, and the OAuth2 / Application Default
     Credentials stack (`internal/oauth2_*_credentials.{h,cc}`).
1. **`google_cloud_cpp_rest_protobuf_internal`**
   (`google/cloud/google_cloud_cpp_rest_protobuf_internal.cmake` -> `.bzl`):
   - Bridges `rest_internal` + `grpc_utils` + Protobuf JSON transcoding
     (`google.longrunning.Operation`, `google.rpc.Status`) for REST-enabled
     GAPIC libraries (`rest_stub_helpers.h`, `async_rest_retry_loop.h`,
     `async_rest_polling_loop.h`, `unified_rest_credentials.h`).
1. **`google_cloud_cpp_universe_domain`**
   (`google/cloud/google_cloud_cpp_universe_domain.cmake` -> `.bzl`, Bazel
   `//:universe_domain`) and **`google/cloud/opentelemetry/`** (Bazel
   `//:opentelemetry`, which links `//:trace`, `//:monitoring`,
   `@opentelemetry-cpp//sdk/src/metrics`, and
   `@opentelemetry-cpp//sdk/src/trace`).
1. **`google_cloud_cpp_mocks`** (`google/cloud/mocks/`): Header-only public
   mock helpers (`mocks::MakeStreamRange<T>`, `current_options.h`,
   `mock_async_streaming_read_write_rpc.h`).

## `testing_util` Target Split

`google/cloud/testing_util/` mirrors the core transport split so non-gRPC and
non-REST unit tests avoid extra link dependencies:

- **`google_cloud_cpp_testing`** (`google/cloud/testing_util/CMakeLists.txt` ->
  `google_cloud_cpp_testing.bzl`): Transport-neutral matchers and utilities
  (`status_matchers.h`, `is_proto_equal.h`, `ScopedLog`, `ScopedEnvironment`,
  `FakeClock`, `AsyncSequencer`, `expect_exception.h`, `expect_future_error.h`,
  `opentelemetry_matchers.h` guarded by `GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY`).
- **`google_cloud_cpp_testing_grpc`**
  (`google/cloud/testing_util/google_cloud_cpp_testing_grpc.cmake` -> `.bzl`):
  `FakeCompletionQueueImpl`, `MockAsyncResponseReader`,
  `MockCompletionQueueImpl`, `ValidateMetadataFixture`.
- **`google_cloud_cpp_testing_rest`**
  (`google/cloud/testing_util/google_cloud_cpp_testing_rest.cmake` -> `.bzl`):
  `MockRestClient`, `MockRestResponse`, `MockHttpPayload`.

## Core Design Invariants

- **CMake -> `.bzl` Synchronization:** Adding or removing any `.h`, `.cc`, or
  `*_test.cc` file requires updating the owning
  `google_cloud_cpp_<sublib>.cmake` (or `testing_util` CMake file) and
  regenerating the matching `.bzl` file via CMake configure. Note that
  `google/cloud/CMakeLists.txt` intentionally does not recurse into service
  subdirectories.
- **Credential Refresh & Retries:** Credential token refresh failures do **not**
  run a nested retry loop; they participate directly in the invoking RPC's retry
  loop (`doc/adr/2021-05-18-credential-failures-participate-in-retry-loop.md`).
- **`StatusOr<T>` Invariants:** Constructing `StatusOr<T>` from an OK `Status`
  is invalid and invokes `internal::ThrowInvalidArgument`. Dereferencing `*s` or
  `s->` when `!s.ok()` is undefined behavior; `.value()` calls
  `internal::ThrowStatus`.
- **`future<T>` Extensions:** Preserve cooperative cancellation callbacks
  (`promise<T>(cancellation_callback)` / `future::cancel()`), `.then()`
  unwrapping semantics, and C++20 coroutine `co_await` compatibility
  (`GOOGLE_CLOUD_CPP_HAVE_COROUTINES`).
- **Verification:** Run core unit tests with:
  `bazelisk test //google/cloud/... --test_tag_filters=-integration-test`
  *(or target core specifically via `//google/cloud:all` +
  `//google/cloud/testing_util/...` + `//google/cloud/mocks/...` +
  `//google/cloud/opentelemetry/...` + `//google/cloud/universe_domain/...`).*
