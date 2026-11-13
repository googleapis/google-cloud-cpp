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
> - [How-to Guide: Adding generated libraries](../../../doc/contributor/howto-guide-adding-generated-libraries.md)
> - [How-to Guide: Updating googleapis SHA](../../../doc/contributor/howto-guide-update-googleapis-sha.md)
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
1. **`PiperOrigin-RevId`**: Identifies the `googleapis` commit that exported the
   service protos (e.g., `PiperOrigin-RevId: 966248502`), used in Step 2 to
   verify the `googleapis` SHA pin.
1. **Service Details** (from the service YAML and `.proto` files; if
   `@googleapis` is not yet fetched in Bazel, inspect them in a Google3
   workspace under `third_party/googleapis/stable/<product_path>/` or
   `third_party/googleapis/current/<product_path>/`):
   - **Library name (`<library>`)**: From `api_short_name` in the YAML (e.g.,
     `biglake`, `designcenter`, `saasservicemgmt`).
   - **Library Display Name (`<Library Display Name>`)**: From `title` in the
     YAML (e.g., `Design Center API`).
   - **Proto directory (`<proto_dir>`)**: Directory in `@googleapis` containing
     the service YAML and `.proto` files (e.g.,
     `google/cloud/saasplatform/saasservicemgmt/v1`).
   - **Product path (`<product_path>`)**: Target directory in
     `google-cloud-cpp`, where the 3rd path component **must** be `<library>`
     (e.g., `google/cloud/saasservicemgmt/v1`, or `google/cloud/biglake/hive/v1`
     when `<library>` has a sub-directory). In
     `generator/internal/scaffold_generator.cc`, `ParseProductPath` extracts the
     library name from the 3rd component of `product_path`, while the service
     YAML path is derived from `service_proto_path`.
   - **Service Proto path(s)**: `<proto_dir>/<service>.proto` for each gRPC
     service listed under `apis:` in the service YAML (some libraries define
     multiple services across multiple `.proto` files, e.g.,
     `deployments_service.proto` and `rollouts_service.proto`).
   - **Launch stage**: `GA` (e.g., `v1` with `launch_stage: GA`) vs
     `EXPERIMENTAL` (e.g., `v1alpha`, `v1beta`, or pre-GA).

______________________________________________________________________

## 2. Verify `googleapis` Pin & Inspect Service Configuration

1. **Query the C++ gRPC Rule**:

   ```bash
   bazelisk query --noshow_progress --noshow_loading_progress \
     "kind(cc_library, @googleapis//<product_path>/...)"
   ```

   Note the exact target name (e.g.,
   `@googleapis//google/cloud/biglake/hive/v1:hive_cc_grpc`).

   - **If the query fails with
     `no such package '@@googleapis+//<product_path>'`**: The `googleapis`
     archive pinned in `MODULE.bazel` predates the `PiperOrigin-RevId` in the
     bug. Before proceeding:
     1. Check open PRs in `googleapis/google-cloud-cpp` (e.g.,
        `chore: update googleapis SHA...`) and
        `bazelbuild/bazel-central-registry` (or branch `regen_bcr` in
        `google-cloud-cpp-bot/bazel-central-registry`) for an in-flight
        `googleapis` update that includes the commit for `PiperOrigin-RevId`.
     1. Either wait for the `googleapis` SHA update PR to merge into `main`, or
        follow
        [How-to Guide: Updating googleapis SHA](../../../doc/contributor/howto-guide-update-googleapis-sha.md)
        in a **separate temporary commit** (updating `MODULE.bazel`,
        `bazel/workspace0.bzl`, `cmake/GoogleapisConfig.cmake`, and adding
        `--registry=https://raw.githubusercontent.com/google-cloud-cpp-bot/bazel-central-registry/regen_bcr/`
        plus `--registry=https://bcr.bazel.build` to `.bazelrc` if the BCR PR is
        not yet merged) so you can generate and validate immediately, then drop
        that temporary commit on rebase once the SHA update merges.

1. **Materialize External Repos in the Bazel Output Base**:
   `bazelisk info output_base` and `bazelisk query` do not materialize
   `@protobuf+` on disk, which `//generator:google-cloud-cpp-codegen` requires
   in Step 3.4. Fetch both external repositories first:

   ```bash
   bazel_output_base="$(bazelisk info output_base)"
   bazelisk fetch "@googleapis//<product_path>/..." "@protobuf//:protobuf_headers"
   ```

1. **Determine Retryable Status Codes**: Inspect
   `<bazel_output_base>/external/googleapis+/<product_path>/*_grpc_service_config.json`.
   Map status codes to C++ enum values:

   - `UNAVAILABLE` -> `"kUnavailable"`
   - `DEADLINE_EXCEEDED` -> `"kDeadlineExceeded"`
   - `RESOURCE_EXHAUSTED` -> `"kResourceExhausted"`
   - `UNAUTHENTICATED` -> `"kUnauthenticated"`

______________________________________________________________________

## 3. Step-by-Step Implementation

### Step 3.1: Check Out Branch

```bash
git checkout -b feat-<library>-generate-library
```

### Step 3.2: Update Scripts and Generator Config

1. **Edit
   [external/googleapis/update_libraries.sh](../../../external/googleapis/update_libraries.sh)**:
   Add the library mapping in alphabetical order to `LIBRARIES`:

   ```bash
   ["<library>"]="@googleapis//<proto_dir>:<rule_name>_cc_grpc"
   ```

1. **Edit
   [generator/generator_config.textproto](../../../generator/generator_config.textproto)**:
   Add a `service` configuration block in alphabetical order for each service
   `.proto` file (if the YAML `apis:` section lists multiple services across
   separate `.proto` files, add one `service` block per `.proto` file sharing
   the same `product_path`):

   ```textproto
   # <Library Display Name>
   service {
     service_proto_path: "<proto_dir>/<service>.proto"
     product_path: "<product_path>"
     initial_copyright_year: "<YYYY>"
     retryable_status_codes: ["kUnavailable", ...]
   }
   ```

### Step 3.3: Generate Proto Lists & Dependencies

```bash
external/googleapis/update_libraries.sh "<library>"
```

Inspect `external/googleapis/protodeps/<library>.deps` for any cross-library
proto dependencies (entries matching
`@googleapis//google/cloud/<other_lib>/...`, such as
`@googleapis//google/cloud/apphub/v1:apphub_proto` or
`@googleapis//google/cloud/documentai/v1:documentai_proto`). If any exist,
configure them in Step 3.5.

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

_(Add `--experimental_scaffold` if the library launch stage is not GA. Note that
`google-cloud-cpp-codegen` regenerates all services in
`generator_config.textproto`; if you are working on top of a temporary
`googleapis` SHA commit before the upstream SHA update PR merges, discard any
regenerated diffs in unrelated libraries via `git checkout -- <other_paths>`)._

### Step 3.5: Fix Build & Cross-Library Dependencies

1. **Bazel (`google/cloud/<library>/BUILD.bazel`)**: Ensure `googleapis_deps`
   uses the exact gRPC target from Step 2 (e.g., `:hive_cc_grpc` instead of
   default `:<library>_cc_grpc`).
1. **Cross-Library Proto Dependencies**: If
   `external/googleapis/protodeps/<library>.deps` contains any
   `@googleapis//google/cloud/<other_lib>/...` entries, update all three of the
   following so CMake and standalone builds can resolve `<other_lib>`'s protos:
   - **`google/cloud/<library>/CMakeLists.txt`**: Add
     `CROSS_LIB_DEPS "<other_lib1>" ...` to
     `google_cloud_cpp_add_gapic_library(...)`:
     ```cmake
     google_cloud_cpp_add_gapic_library(
         <library> "<Library Display Name>"
         SERVICE_DIRS "<service_dir>/"
         CROSS_LIB_DEPS "<other_lib1>" "<other_lib2>")
     ```
   - **[cmake/CompileProtos.cmake](../../../cmake/CompileProtos.cmake)**: In
     `google_cloud_cpp_load_protodeps`, verify `target_substitutions` includes
     `"cloud_<other_lib>_v1_<other_lib>_protos\;<other_lib>_protos"` (add it if
     missing). Without this, CMake fails trying to link
     `google-cloud-cpp::cloud_<other_lib>_..._protos`.
   - **[cmake/GoogleCloudCppFeatures.cmake](../../../cmake/GoogleCloudCppFeatures.cmake)**:
     In `google_cloud_cpp_enable_deps`, add:
     ```cmake
     if (feature STREQUAL "<library>")
         google_cloud_cpp_enable_directly_used_library(<other_lib>)
     endif ()
     ```
     so standalone builds (`-DGOOGLE_CLOUD_CPP_ENABLE=<library>`, used by
     `check-api-pr`) automatically enable `<other_lib>`.

### Step 3.6: Update Root Feature Lists

1. **[cmake/GoogleCloudCppFeatures.cmake](../../../cmake/GoogleCloudCppFeatures.cmake)**:
   Add `"<library>"` in alphabetical order to `GOOGLE_CLOUD_CPP_GA_LIBRARIES`
   (or `GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES`).
1. **[libraries.bzl](../../../libraries.bzl)**: Add `"<library>"` in
   alphabetical order to `GOOGLE_CLOUD_CPP_GA_LIBRARIES` (or
   `GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES`).

### Step 3.7: Implement Quickstart

Inspect the generated `google/cloud/<library>/<service_dir>/<service>_client.h`
to pick a simple top-level `List*` RPC and check its `parent` resource pattern:

- **Project-scoped parent (`projects/*`)**:
  1. **`google/cloud/<library>/quickstart/quickstart.cc`**: Replace placeholder
     `#include` with the client header, check `argc != 2` (`project-id`), and
     pass `google::cloud::Project(argv[1]).FullName()`.
  1. **`google/cloud/<library>/CMakeLists.txt`**: Pass `GOOGLE_CLOUD_PROJECT` in
     `add_test` for `<library>_quickstart`.
  1. **`google/cloud/<library>/quickstart/README.md`**: Replace placeholder
     `[...]` with `[project-id]`.
- **Location-scoped / regional parent (`projects/*/locations/*`)**:
  1. **`google/cloud/<library>/quickstart/quickstart.cc`**: Replace placeholder
     `#include` with the client header and `#include "google/cloud/location.h"`,
     check `argc != 3` (`project-id location-id`), and pass
     `google::cloud::Location(argv[1], argv[2]).FullName()`.
  1. **`google/cloud/<library>/CMakeLists.txt`**: Pass
     `GOOGLE_CLOUD_PROJECT GOOGLE_CLOUD_CPP_TEST_REGION` in `add_test` for
     `<library>_quickstart`.
  1. **`google/cloud/<library>/quickstart/README.md`**: Replace placeholder
     `[...]` with `[project-id] [location-id]`.

Enable the service API in the CI test project (`cloud-cpp-testing-resources`) so
the `quickstart-production-pr` CI check can call the endpoint:

```bash
gcloud services enable --project=cloud-cpp-testing-resources "<library>.googleapis.com"
```

(Or enable it in the Cloud Console at
`https://console.developers.google.com/apis/api/<library>.googleapis.com/overview?project=cloud-cpp-testing-resources`.
If the API is restricted/private and cannot be enabled in
`cloud-cpp-testing-resources`, add `DISABLED "True"` to `set_tests_properties`
for `<library>_quickstart` in `google/cloud/<library>/CMakeLists.txt`).

If `quickstart-production-pr` still fails after enabling the API with a
service-side `PERMISSION_DENIED` or `INVALID_ARGUMENT` permanent error (for
example, when a regional CCFE service's global gRPC endpoint routes to
`prod-global` without `grpc_multi_api_scope`, returning `Read/Write` access
denied on `cloud-cpp-testing-resources`, or when extra project setup is
required), configure `PASS_REGULAR_EXPRESSION` on `<library>_quickstart` in
`google/cloud/<library>/CMakeLists.txt`:

```cmake
    set_tests_properties(
        <library>_quickstart
        PROPERTIES LABELS "integration-test;quickstart" PASS_REGULAR_EXPRESSION
                   "Permanent error.*gcloud-cpp.retry.function=<ListMethod>")
```

### Step 3.8: Update Documentation & Changelog

1. **[CHANGELOG.md](../../../CHANGELOG.md)**: Add
   `- [<Library Display Name>](/google/cloud/<library>/README.md)` under
   `New Libraries` in the upcoming release section.
1. Ensure `google/cloud/<library>/<product_subpath>/.repo-metadata.json` exists
   and is tracked.

______________________________________________________________________

## 4. Format & Validate

1. **Stage all files and run checkers**:

   ```bash
   git add external generator ci cmake libraries.bzl CHANGELOG.md README.md "google/cloud/<library>"
   ci/cloudbuild/build.sh -t checkers-pr
   ```

   _(Re-run if formatters or documentation scripts modified files such as
   `README.md` until `checkers-pr` passes cleanly with exit code 0 and no git
   diff)._

1. **Verify Bazel Build & Sample Tests**:

   ```bash
   bazelisk build //google/cloud/<library>/...
   bazelisk test --test_arg=set-client-endpoint //google/cloud/<library>/...
   ```

   _(`--test_arg=set-client-endpoint` is required so the generated
   `*_client_samples` test target runs its offline endpoint sample instead of
   failing with missing integration-test arguments)._

1. **Create and Verify API Baseline (GA libraries only)**:

   ```bash
   env GOOGLE_CLOUD_CPP_CHECK_API=<library> ci/cloudbuild/build.sh -t check-api-pr
   git add ci/abi-dumps
   env GOOGLE_CLOUD_CPP_CHECK_API=<library> ci/cloudbuild/build.sh -t check-api-pr
   ```

1. **Verify CMake Installation & Quickstart**:

   ```bash
   ci/cloudbuild/build.sh -t cmake-install-pr
   ```

   _(Or, for a faster targeted verification after `check-api-pr` has already
   built `cmake-out` inside the `gcb-fedora-latest-cmake:latest` container, run
   the install, `pkg-config --validate`, and quickstart build inside that same
   container where `/workspace` matches the configured CMake paths):_

   ```bash
   docker run --rm -u "$(id -u):$(id -g)" \
     -v "${PWD}:/workspace" \
     -v "${PWD}/build-out/fedora-latest-cmake/check-api/__default__/cmake-out:/workspace/cmake-out" \
     -w /workspace gcb-fedora-latest-cmake:latest bash -ec '
       cmake --install /workspace/cmake-out --prefix /tmp/staging >/dev/null
       env PKG_CONFIG_PATH="/tmp/staging/lib64/pkgconfig:/tmp/staging/lib/pkgconfig:${PKG_CONFIG_PATH:-}" \
         pkg-config --validate google_cloud_cpp_<library> google_cloud_cpp_<library>_protos
       cmake -H"google/cloud/<library>/quickstart" -B"/tmp/quickstart-<library>" \
         -DCMAKE_PREFIX_PATH="/tmp/staging"
       cmake --build "/tmp/quickstart-<library>"
     '
   ```

1. **Verify Full Generator Pipeline**:

   ```bash
   ci/cloudbuild/build.sh -t generate-libraries-pr
   ```

______________________________________________________________________

## 5. Commit Changes

Commit all library files in a single commit (excluding any temporary SHA
override files from Step 2):

```bash
git add external generator ci cmake libraries.bzl CHANGELOG.md README.md "google/cloud/<library>"
git commit -m "feat(<library>): generate library"
```
