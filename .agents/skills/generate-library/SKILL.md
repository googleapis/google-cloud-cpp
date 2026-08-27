---
name: generate-library
description: Generates a new C++ client library for google-cloud-cpp from a Buganizer library generation request. Use when asked to generate, scaffold, or onboard a new Google Cloud service or library, or when given a Buganizer issue ID (e.g., b/123456789).
---

# Generate New Library from Buganizer Request

This skill guides the end-to-end process of generating and validating a new C++
client library in `google-cloud-cpp` from a Buganizer generation request
(b/...).

> **Reference Documentation**:
>
> - [How-to Guide: Adding generated libraries](doc/contributor/howto-guide-adding-generated-libraries.md)
> - [How-to Guide: Updating googleapis SHA](doc/contributor/howto-guide-update-googleapis-sha.md)
>   (if the proto dependency is not yet available at the pinned googleapis SHA)

______________________________________________________________________

## 1. Parse the Buganizer Request

Fetch the issue details using the Buganizer CLI:

```bash
/google/bin/releases/issues-cli/issues render <BUG_ID>
```

Extract the following information:

1. **Service YAML / Proto Path**: e.g.,
   `google/cloud/biglake/hive/v1/biglake_v1.yaml`
1. **`PiperOrigin-RevId`**: Required in the commit description (e.g.,
   `PiperOrigin-RevId: 966248502`).
1. **Service Details**:
   - Library name (e.g., `biglake` from `api_short_name` in the YAML).
   - Product path (e.g., `google/cloud/biglake/hive/v1`).
   - Service Proto path (e.g.,
     `google/cloud/biglake/hive/v1/hive_metastore.proto`).
   - Launch stage (`GA` vs `EXPERIMENTAL`).

______________________________________________________________________

## 2. Inspect Googleapis Rules and Service Configuration

1. **Find the Bazel Output Base**:

   ```bash
   bazel_output_base="$(bazelisk info output_base)"
   ```

1. **Query the C++ gRPC Rule**:

   ```bash
   bazelisk query --noshow_progress --noshow_loading_progress \
     "kind(cc_library, @googleapis//<product_path>/...)"
   ```

   Note the exact target name (e.g.
   `@googleapis//google/cloud/biglake/hive/v1:hive_cc_grpc`).

1. **Determine Retryable Status Codes**: Inspect
   `<bazel_output_base>/external/googleapis+/<product_path>/*_grpc_service_config.json`.
   Map status codes to C++ enum values:

   - `UNAVAILABLE` -> `"kUnavailable"`
   - `DEADLINE_EXCEEDED` -> `"kDeadlineExceeded"`
   - `RESOURCE_EXHAUSTED` -> `"kResourceExhausted"`
   - `UNAUTHENTICATED` -> `"kUnauthenticated"`

______________________________________________________________________

## 3. Step-by-Step Implementation

### Step 3.1: Update Scripts and Generator Config

1. **Edit
   [external/googleapis/update_libraries.sh](external/googleapis/update_libraries.sh)**:
   Add the library mapping in alphabetical order to `LIBRARIES`:

   ```bash
   ["<library>"]="@googleapis//<product_path>:<rule_name>_cc_grpc"
   ```

1. **Edit
   [generator/generator_config.textproto](generator/generator_config.textproto)**:
   Add the service configuration block in alphabetical order:

   ```textproto
   # <Library Display Name>
   service {
     service_proto_path: "<product_path>/<service>.proto"
     product_path: "<product_path>"
     initial_copyright_year: "<YYYY>"
     retryable_status_codes: ["kUnavailable", ...]
   }
   ```

### Step 3.2: Check Out Branch & Commit Initial Config

```bash
git checkout -b feat-<library>-generate-library
git commit -m "feat(<library>): generate library" external/ generator/
```

### Step 3.3: Generate Proto Lists & Dependencies

```bash
external/googleapis/update_libraries.sh "<library>"
```

### Step 3.4: Run Scaffold Generator

```bash
bazelisk run \
  //generator:google-cloud-cpp-codegen -- \
  --protobuf_proto_path="${bazel_output_base}/external/protobuf+/src" \
  --googleapis_proto_path="${bazel_output_base}/external/googleapis+" \
  --discovery_proto_path="${PWD}/protos" \
  --output_path="${PWD}" \
  --config_file="${PWD}/generator/generator_config.textproto" \
  --scaffold_templates_path="${PWD}/generator/templates/" \
  --scaffold="google/cloud/<library>/"
```

_(Add `--experimental_scaffold` if the library launch stage is not GA)._

### Step 3.5: Fix Build Dependencies

Verify `google/cloud/<library>/BUILD.bazel`: Ensure `googleapis_deps` uses the
exact gRPC target from Step 2 (e.g., `:hive_cc_grpc` instead of default
`:<library>_cc_grpc`).

### Step 3.6: Update Root Feature Lists

1. **[cmake/GoogleCloudCppFeatures.cmake](cmake/GoogleCloudCppFeatures.cmake)**:
   Add `"<library>"` in alphabetical order to `GOOGLE_CLOUD_CPP_GA_LIBRARIES`
   (or `GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES`).
1. **[libraries.bzl](libraries.bzl)**: Add `"<library>"` in alphabetical order
   to `GOOGLE_CLOUD_CPP_GA_LIBRARIES` (or
   `GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES`).

### Step 3.7: Implement Quickstart

1. **`google/cloud/<library>/quickstart/quickstart.cc`**:
   - Replace placeholder `#include` with the primary client header.
   - Call a simple top-level list/get RPC (e.g., `ListCatalogs` or
     `ListResources`).
1. **`google/cloud/<library>/CMakeLists.txt`**:
   - Update test arguments in add_test for <library>\_quickstart (e.g.
     `GOOGLE_CLOUD_PROJECT`).
1. **`google/cloud/<library>/quickstart/README.md`**:
   - Replace placeholder `[...]` command-line arguments.

### Step 3.8: Update Documentation & Changelog

1. **[CHANGELOG.md](CHANGELOG.md)**: Add the library under `New Libraries` in
   the upcoming release section.
1. Ensure `google/cloud/<library>/<product_subpath>/.repo-metadata.json` exists
   and is tracked.

______________________________________________________________________

## 4. Format & Validate

1. **Stage all files and run checkers**:

   ```bash
   git add external ci cmake libraries.bzl CHANGELOG.md README.md "google/cloud/<library>"
   ci/cloudbuild/build.sh -t checkers-pr
   ```

   _(Re-run if formatters or documentation scripts made changes until checkers
   pass cleanly with exit code 0)._

1. **Verify Bazel Build**:

   ```bash
   bazelisk build //google/cloud/<library>/...
   ```

1. **Verify Full CMake Installation & Quickstart**:

   ```bash
   ci/cloudbuild/build.sh -t cmake-install-pr
   ```

1. **Verify Full Generator Pipeline**:

   ```bash
   ci/cloudbuild/build.sh -t generate-libraries-pr
   ```

1. **Create and Verify API Baseline (GA libraries only)**:

   ```bash
   env GOOGLE_CLOUD_CPP_CHECK_API=<library> ci/cloudbuild/build.sh -t check-api-pr
   git add ci/abi-dumps
   env GOOGLE_CLOUD_CPP_CHECK_API=<library> ci/cloudbuild/build.sh -t check-api-pr
   ```

______________________________________________________________________

## 5. Commit Changes

Commit all files with the `PiperOrigin-RevId` extracted from the Buganizer
request:

```bash
git add -A
git commit -m "feat(<library>): add <Library Display Name> C++ client library

PiperOrigin-RevId: <REV_ID>"
```
