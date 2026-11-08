# Agent Guidelines for BigQuery (`google/cloud/bigquery/`)

`google/cloud/bigquery/` bundles two distinct libraries (`//:bigquery` and
`//:experimental-bigquery-rest`), while the GA BigQuery v2 REST control-plane
library lives in a separate repository directory.

## Critical Disambiguation: `bigquery` vs. `bigquerycontrol`

- **GA BigQuery v2 Control-Plane REST APIs (`DatasetService`, `JobService`,
  `ModelService`, `ProjectService`, `RoutineService`, `RowAccessPolicyService`,
  `TableService`) do NOT live here.** They live in sibling
  **`google/cloud/bigquerycontrol/`** (`//:bigquerycontrol`,
  `google-cloud-cpp::bigquerycontrol`).

## Dual Library Structure in `google/cloud/bigquery/`

1. **`//:bigquery`** (`bigquery_grpc.cmake`, CMake
   `-DGOOGLE_CLOUD_CPP_ENABLE=bigquery` -> `google-cloud-cpp::bigquery` &
   `google-cloud-cpp::bigquery_mocks`):
   - **Generated GAPIC gRPC services** across nine subdirectories:
     `analyticshub/v1/`, `biglake/v1/`, `connection/v1/`, `datapolicies/v1/`,
     `datapolicies/v2/`, `datatransfer/v1/`, `migration/v2/`, `reservation/v1/`,
     and `storage/v1/` (`BigQueryReadClient` and `BigQueryWriteClient` in
     `namespace google::cloud::bigquery_storage_v1`), plus generated top-level
     compatibility headers (`bigquery/*.h`).
   - **Do not hand-edit** generated GAPIC files (see root `AGENTS.md`). The
     custom `ReadRows` resume updater in
     `storage/v1/internal/bigquery_read_streaming.cc` is hand-authored via
     `omit_streaming_updater: true` in `generator/generator_config.textproto`.
1. **`//:experimental-bigquery-rest`** (`bigquery_rest.cmake`, CMake
   `-DGOOGLE_CLOUD_CPP_ENABLE=experimental-bigquery_rest` ->
   `google-cloud-cpp::experimental-bigquery_rest` &
   `google-cloud-cpp::experimental-bigquery_rest_mocks`):
   - **Hand-crafted experimental REST/JSON clients** in `v2/minimal/`
     (`JobClient`, `DatasetClient`, `ProjectClient`, `TableClient`) built on
     `google_cloud_cpp_rest_internal` and `nlohmann::json` without Protobuf, in
     `namespace google::cloud::bigquery_v2_minimal_internal` and
     `bigquery_v2_minimal_mocks`.
   - When adding or removing files under `v2/minimal/`, update
     `bigquery_rest.cmake` and regenerate its `.bzl` lists via CMake configure.

## Running Tests

- Unit tests (both libraries): `bazelisk test`
  `--test_tag_filters=-integration-test //google/cloud/bigquery/...`
- Note: Use `//:bigquery_mocks` (underscore); `//:bigquery-mocks` (hyphen) is a
  deprecated alias.
