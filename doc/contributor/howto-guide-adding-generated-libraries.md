# How-to Guide: Adding generated libraries

This document describes the steps required to add a new library to
`google-cloud-cpp`. The document is intended for contributors to the
`google-cloud-cpp` libraries, it assumes you are familiar with the build systems
used in these libraries, that you are familiar with existing libraries, and with
which libraries are based on gRPC.

> :warning: for libraries that include multiple services, the scaffold README
> files (and any other documentation) will use the **last** service description
> as the description of the library. Adjust the ordering and/or fix the
> documentation after the fact.

## Set your working directory

Go to whatever directory holds your clone of the project, for example:

```shell
cd $HOME/google-cloud-cpp
```

## Set some useful variables

```shell
library=... # The name of your new library in the google-cloud-cpp repository
subdir="google/cloud/${library}"  # The path in googleapis repo, may not start with google/cloud/
```

## Verify the C++ rules exist

```shell
bazel --batch query --noshow_progress --noshow_loading_progress \
    "kind(cc_library, @com_google_googleapis//${subdir}/...)"
```

If this fails, send a CL to add the rule. Wait until that is submitted and
exported before proceeding any further.

## Edit the scripts and configuration

Update the `external/googleapis/update_libraries.sh` script.

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

## Update the Generator Configuration

Determine the retryable status codes by looking in the service config JSON. For
example, [here][retryable-status-codes].

Manually edit `generator/generator_config.textproto` and add the new service.

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
+  product_path: "google/cloud/secretmanager"
+  initial_copyright_year: "2021"
+  retryable_status_codes: ["kDeadlineExceeded", "kUnavailable"]
+}
+
```

</details>

## Commit these changes

Create your first commit with purely hand-crafted changes

```shell
git checkout -b feat-${library}-generate-library
git commit -m"feat(${library}): generate library" external/ generator/
```

## Update the list of proto files and proto dependencies

```shell
external/googleapis/update_libraries.sh "${library}"
```

## Run the Scaffold Generator

Then run the micro-generator to create the scaffold and the C++ sources:

```shell
bazel_output_base="$(bazel info output_base)"
bazel run \
  //generator:google-cloud-cpp-codegen -- \
  --protobuf_proto_path="${bazel_output_base}"/external/com_google_protobuf/src \
  --googleapis_proto_path="${bazel_output_base}"/external/com_google_googleapis \
  --output_path="${PWD}" \
  --config_file="${PWD}/generator/generator_config.textproto" \
  --scaffold="google/cloud/${library}"
```

To generate a library that is initially experimental, add an
`--experimental_scaffold` flag.

## Fix formatting of existing libraries and the generated code

```shell
git add "google/cloud/${library}"
ci/cloudbuild/build.sh -t checkers-pr
```

## Commit all the generated files

```shell
git add external ci "google/cloud/${library}"
git commit -m"Run generators and format their outputs"
```

## Create any custom source files

If the `generator/generator_config.textproto` entry for the service does not
enumerate the `retryable_status_codes`, you need to manually create the file as
`google/cloud/$library/internal/<service_name>_retry_traits.h`.

Likewise, for services using streaming operations you may need to implement the
streaming `*Updater` function. Use `google/cloud/bigquery/internal/streaming.cc`
or `google/cloud/logging/internal/streaming.cc` for inspiration.

## Potentially fix the bazel build

The generated `BUILD.bazel` file may require manual editing. The scaffold will
add one dependency from `@com_github_googleapis//${subdir}`, which might not be
correct. You may need to modify that dependency and/or add additional
dependencies for more complex libraries.

## Update the root files

Manually edit the top level `BUILD.bazel` to include the new target.

<details>
<summary>Expand for an example</summary>

If you are generating a GA library, add it to `GA_LIBRARIES`.

```diff
diff --git a/BUILD.bazel b/BUILD.bazel
index 2c08e2a73..3a61351d2 100644
--- a/BUILD.bazel
+++ b/BUILD.bazel
@@ -52,6 +52,7 @@ GA_LIBRARIES = [
     "automl",
     "billing",
     "binaryauthorization",
+    "bms",
     "channel",
     "cloudbuild",
     "composer",
```

Otherwise, if you are generating an experimental library, add it to
`EXPERIMENTAL_LIBRARIES` and take note of when the library was generated.

```diff
diff --git a/BUILD.bazel b/BUILD.bazel
index e43d85b64..1c35cf61d 100644
--- a/BUILD.bazel
+++ b/BUILD.bazel
@@ -21,6 +21,8 @@ exports_files([
 ])

 EXPERIMENTAL_LIBRARIES = [
+    # Introduced in 2022-05
+    "bms",
     # Introduced in 2022-04
     "dataplex",
     "dialogflow_cx",
```

</details>

## Update the quickstart

The generated quickstart will need some editing. Use a simple operation, maybe
an admin operation listing top-level resources, to demonstrate how to use the
API. Test your changes with:

```sh
bazel run -- //google/cloud/${library}/quickstart:quickstart $params
```

Edit the tests so this new quickstart receives the right command-line
arguments in the CI builds.

- `google/cloud/${library}/CMakeLists.txt`

The scaffold generator does not know the current version of `google-cloud-cpp`.
We can copy it from a library that should be up-to-date.

```sh
sed -i '/workspace(/q' google/cloud/${library}/quickstart/WORKSPACE.bazel
sed '1,/workspace(/d' google/cloud/accessapproval/quickstart/WORKSPACE.bazel \
    >> google/cloud/${library}/quickstart/WORKSPACE.bazel
```

## Update the README files

The following files probably need some light copy-editing to read less like they
were written by a robot:

- `google/cloud/${library}/README.md`
- `google/cloud/${library}/quickstart/README.md`
- `google/cloud/${library}/doc/main.dox`
- `google/cloud/${library}/doc/options.dox`

The Cloud documentation links (`cloud.google.com/*/docs/*`) in these files are
not always valid. Find the correct urls and update the links.

## Edit the top-level CHANGELOG file

Announce the new library in the CHANGELOG for the next release.

## Fix formatting nits

```shell
ci/cloudbuild/build.sh -t checkers-pr
```

## Verify everything compiles

```shell
bazel build //google/cloud/${library}/...
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

## Commit these changes

```shell
git commit -m"Manually update READMEs, quickstart, and top-level stuff" \
   "google/cloud/${library}" BUILD.bazel CHANGELOG.md ci README.md
```

[retryable-status-codes]: https://github.com/googleapis/googleapis/blob/0fea253787a4f2769b97b0ed3a8f5b28ef17ffa7/google/cloud/secretmanager/v1/secretmanager_grpc_service_config.json#L77-L80
