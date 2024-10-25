# How-to Guide: Adding generated libraries

This document describes the steps required to add a new library to
`google-cloud-cpp`. It is intended for contributors, and assumes you are
familiar with the existing libraries, the build systems used in those libraries,
and which libraries are based on gRPC.

- [Adding a new library](#adding-a-new-library)
- [Expanding a library](#expanding-a-library)

## Adding a new library

> :warning: For libraries that include multiple services, the scaffold README
> files (and any other documentation) will use the **last** service description
> as the description of the library. Adjust the ordering and/or fix the
> documentation after the fact.

### Set your working directory

Go to whatever directory holds your clone of the project, for example:

```shell
cd $HOME/google-cloud-cpp
```

### Set some useful variables

```shell
library=... # The name of your new library in the google-cloud-cpp repository
subdir="google/cloud/${library}"  # The path in googleapis repo, may not start with google/cloud/
bazel_output_base="$(bazelisk info output_base)"
```

### Verify the C++ rules exist

You may need to
[Send a PR to update the googleapis SHA to the latest version](../contributor/howto-guide-update-googleapis-sha.md).
Wait until that is submitted before proceeding any further for the following 2
cases:

1. The dependency does not exist at the pinned version of the googleapis repo.

```shell
bazelisk --batch query --noshow_progress --noshow_loading_progress \
    "kind(cc_library, @com_google_googleapis//${subdir}/...)"
```

- If the command fails, it returns something like this:

```shell
ERROR: no targets found beneath 'commerce'
```

2. Check `$bazel_output_base/external/googleapis~/api-index-v1.json`, if
   `$library` with the correct version is not in `apis` section.

### Edit the scripts and configuration

Update the
[external/googleapis/update_libraries.sh](../../external/googleapis/update_libraries.sh)
script.

<details>
<summary>Expand for an example</summary>

```diff
diff --git a/external/googleapis/update_libraries.sh b/external/googleapis/update_libraries.sh
index cdaa0bc9f..b0381d72d 100755
--- a/external/googleapis/update_libraries.sh
+++ b/external/googleapis/update_libraries.sh
@@ -40,6 +40,11 @@ declare -A -r LIBRARIES=(
   ["logging"]="@com_google_googleapis//google/logging/v2:logging_cc_grpc"
   ["monitoring"]="@com_google_googleapis//google/monitoring/v3:monitoring_cc_grpc"
   ["pubsub"]="@com_google_googleapis//google/pubsub/v1:pubsub_cc_grpc"
+  ["secretmanager"]="$(
+    printf ",%s" \
+      "@com_google_googleapis//google/cloud/secretmanager/v1:secretmanager_cc_grpc" \
+      "@com_google_googleapis//google/cloud/secretmanager/logging/v1:logging_cc_grpc"
+  )"
   ["spanner"]="$(
     printf ",%s" \
       "@com_google_googleapis//google/spanner/v1:spanner_cc_grpc" \
```

</details>

### Update the Generator Configuration

Determine the retryable status codes by looking in the service config JSON. For
example, [here][retryable-status-codes].

Manually edit
[generator/generator_config.textproto](../../generator/generator_config.textproto)
and add the new service.

Find the list of `.proto` files that will need to be included:

```shell
find "${bazel_output_base}/external/googleapis~/${subdir}" -name '*.proto' -print0 |
  xargs -0 grep -l '^service'
```

> **Note:** While older service definitions may not include the version
> specification in the `product_path` field, all new services are required to
> include the version.

<details>
<summary>Expand for an example</summary>

```diff
diff --git a/generator/generator_config.textproto b/generator/generator_config.textproto
index ab033dde9..3753287d8 100644
--- a/generator/generator_config.textproto
+++ b/generator/generator_config.textproto
@@ -78,6 +78,14 @@ service {

 }

+# Secret Manager
+service {
+  service_proto_path: "google/cloud/secretmanager/v1/service.proto"
+  product_path: "google/cloud/secretmanager/v1"
+  initial_copyright_year: "2021"
+  retryable_status_codes: ["kUnavailable"]
+}
+
```

</details>

### Commit these changes

Create your first commit with purely hand-crafted changes

```shell
git checkout -b feat-${library}-generate-library
git commit -m"feat(${library}): generate library" external/ generator/
```

### Update the list of proto files and proto dependencies

```shell
external/googleapis/update_libraries.sh "${library}"
```

### Run the Scaffold Generator

Then run the micro-generator to create the scaffold and the C++ sources:

```shell
bazelisk run \
  //generator:google-cloud-cpp-codegen -- \
  --protobuf_proto_path="${bazel_output_base}"/external/protobuf~/src \
  --googleapis_proto_path="${bazel_output_base}"/external/googleapis~ \
  --discovery_proto_path="${PWD}/protos" \
  --output_path="${PWD}" \
  --config_file="${PWD}/generator/generator_config.textproto" \
  --scaffold_templates_path="${PWD}/generator/templates/" \
  --scaffold="google/cloud/${library}/"
```

To generate a library that is initially experimental, add an
`--experimental_scaffold` flag.

### Update the root files

Manually edit
[cmake/GoogleCloudCppFeatures.cmake](../../cmake/GoogleCloudCppFeatures.cmake)
to include the new target. If you are generating a GA library, add it to
`GOOGLE_CLOUD_CPP_GA_LIBRARIES`. Otherwise, if you are generating an
experimental library, add it to `GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES` and
note in a comment when the library was generated.

Update [libraries.bzl](../../libraries.bzl) to include the new library. While
this can be done by running a cmake-based build, it is fastest to edit the file
manually.

### Fix formatting of existing libraries and the generated code

```shell
git add "google/cloud/${library}"
ci/cloudbuild/build.sh -t checkers-pr
```

### Verify the generated changes look right

```shell
tree google/cloud/$library
```

### Commit all the generated files

```shell
git add external ci "google/cloud/${library}" README.md
git commit -m"Run generators and format their outputs"
```

### Create any custom source files

If the `generator/generator_config.textproto` entry for the service does not
enumerate the `retryable_status_codes`, you need to manually create the file as
`google/cloud/$library/internal/<service_name>_retry_traits.h`.

Likewise, for services using streaming operations you may need to implement the
streaming `*Updater` function. Use `google/cloud/bigquery/internal/streaming.cc`
or `google/cloud/logging/internal/streaming.cc` for inspiration.

### Potentially fix the bazel build

The generated `BUILD.bazel` file may require manual editing. The scaffold will
add one dependency from `@com_github_googleapis//${subdir}`, which might not be
correct. You may need to modify that dependency and/or add additional
dependencies for more complex libraries.

### Potentially update the service directories

A library may contain services in several subdirectories. The scaffold only
knows about one such subdirectory. You may need to manually update the
`service_dirs` lists to include all subdirectories in the following files:

- `google/cloud/${library}/BUILD.bazel`
- `google/cloud/${library}/CMakeLists.txt`

[#10237] offers one way to automate this step.

### Update the quickstart

The generated quickstart will need some editing. Use a simple operation, maybe
an admin operation listing top-level resources, to demonstrate how to use the
API. Test your changes with:

```sh
gcloud services enable --project=cloud-cpp-testing-resources "${library}.googleapis.com"
bazelisk run -- //google/cloud/${library}/quickstart:quickstart $params
```

Edit the tests so this new quickstart receives the right command-line arguments
in the CI builds.

- `google/cloud/${library}/CMakeLists.txt`

### Add the API baseline

For new GA libraries you need to create the API baseline. You can leave this
running while you work on tweaks to the quickstart and documentation.

```shell
env GOOGLE_CLOUD_CPP_CHECK_API=${library} ci/cloudbuild/build.sh -t check-api-pr
git add ci/abi-dumps
git commit -m "Add API baseline"
```

The following error is expected for the first run, because the command generates
the ABI dump, it fails because there is no previous ABI dump to compare to. When
you run once more, the error is gone.

```
grep: cmake-out/compat_reports/google_cloud_cpp_parallelstore/src_compat_report.html: No such file or directory
2024-10-25T19:00:49Z (+57s): ABI Compliance error: google_cloud_cpp_parallelstore
```

### Update the README files

The following files probably need some light copy-editing to read less like they
were written by a robot:

- `google/cloud/${library}/README.md`
- `google/cloud/${library}/quickstart/README.md`
- `google/cloud/${library}/doc/main.dox`
- `google/cloud/${library}/doc/options.dox`

The Cloud documentation links (`cloud.google.com/*/docs/*`) in these files are
not always valid. Find the correct urls and update the links.

### Review the metadata file

Newer services provide all the data needed to generate this file correctly, but
with older services we need to edit a few places:

- `google/cloud/${library}/**/.repo-metadata.json`

### Edit the top-level CHANGELOG file

Announce the new library in the [CHANGELOG.md](../CHANGELOG.md) for the next
release.

### Fix formatting nits

```shell
ci/cloudbuild/build.sh -t checkers-pr
```

### Verify everything compiles

```shell
bazelisk build //google/cloud/${library}/...
ci/cloudbuild/build.sh -t cmake-install-pr
```

It is somewhat common for the `cmake-install` build to fail due to expected
install directories. If this happens, make an edit to `cmake-install.sh`.

<details>
<summary>Expand for an example</summary>

```diff
diff --git a/ci/cloudbuild/builds/cmake-install.sh b/ci/cloudbuild/builds/cmake-install.sh
index c4ce00489..1858b48dc 100755
--- a/ci/cloudbuild/builds/cmake-install.sh
+++ b/ci/cloudbuild/builds/cmake-install.sh
@@ -73,6 +73,9 @@ expected_dirs+=(
   ./include/google/cloud/pubsub
   ./include/google/cloud/pubsub/internal
   ./include/google/cloud/pubsub/mocks
+  # no gRPC services in google/cloud/secretmanager/logging
+  ./include/google/cloud/secretmanager/logging
+  ./include/google/cloud/secretmanager/logging/v1
   ./include/google/cloud/spanner/admin/mocks
   ./include/google/cloud/spanner/internal
   ./include/google/cloud/spanner/mocks
```

</details>

### Commit these changes

```shell
git commit -m "Manually update READMEs, quickstart, and top-level stuff" .
```

## Expanding a library

> This section assumes the developer is familiar with running the generator. For
> more details on any of the steps involved, see the "Adding a new library"
> section.

Sometimes we add services to libraries that already exist. In this case we do
not need to run the scaffold generator. We provide a simple checklist below.

### Manual changes

```sh
library=...  # e.g. bigquery
api_name=... # e.g. BigLake API
```

- Check out a new branch
  - `git checkout -b feat-expand-${library}`
- Update `generator_config.textproto`
- Update and run `external/googleapis/update_libraries.sh ${library}`
- Add the new directory to `service_dirs` in:
  - `google/cloud/${library}/CMakeLists.txt`
  - `google/cloud/${library}/BUILD.bazel`
- Add the new `*_cc_grpc` dependency to
  `//google/cloud/${library}:${library}_client` in
  `google/cloud/${library}/BUILD.bazel`
- Review the `google/cloud/${library}/**/.repo-metadata.json` file
- Announce the new API in the `CHANGELOG.md`
  - e.g. `The library has been expanded to include the ${api_name}.`
- Commit the manual changes
  - `git commit -am "feat(${library}): add ${api_name}"`

### Generated changes

```shell
ci/cloudbuild/build.sh -t generate-libraries-pr
env GOOGLE_CLOUD_CPP_CHECK_API=${library} ci/cloudbuild/build.sh -t check-api-pr
git add .
ci/cloudbuild/build.sh -t checkers-pr
git commit -am "generated changes"
```

### Verify everything compiles

```shell
bazelisk build //google/cloud/${library}/...
ci/cloudbuild/build.sh -t cmake-install-pr
```

[#10237]: https://github.com/googleapis/google-cloud-cpp/issues/10237
[retryable-status-codes]: https://github.com/googleapis/googleapis/blob/70147caca58ebf4c8cd7b96f5d569a72723e11c1/google/cloud/secretmanager/v1/secretmanager_grpc_service_config.json#L77-L80
