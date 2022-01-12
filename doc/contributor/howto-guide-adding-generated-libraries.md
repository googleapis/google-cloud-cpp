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

## Set some useful variables

```shell
cd $HOME/google-cloud-cpp
library=... # The name of your new library in the google-cloud-cpp repository
path="google/cloud/${library}"  # The path in googleapis repo, may not start with google/cloud/
```

## Verify the C++ rules exist

```shell
bazel --batch query --noshow_progress --noshow_loading_progress \
    "kind(cc_library, @com_google_googleapis//${path}/...)"
```

If this fails, send a CL to add the rule. Wait until that is submitted and
exported before proceeding any further.

## Edit the scripts and configuration

Update the `external/googleapis/update_libraries.sh` script, for example:

```diff
diff --git a/external/googleapis/update_libraries.sh b/external/googleapis/update_libraries.sh
index cdaa0bc9f..b0381d72d 100755
--- a/external/googleapis/update_libraries.sh
+++ b/external/googleapis/update_libraries.sh
@@ -40,6 +40,11 @@ declare -A -r LIBRARIES=(
   ["logging"]="@com_google_googleapis//google/logging/v2:logging_proto"
   ["monitoring"]="@com_google_googleapis//google/monitoring/v3:monitoring_proto"
   ["pubsub"]="@com_google_googleapis//google/pubsub/v1:pubsub_proto"
+  ["secretmanager"]="$(
+    printf ",%s" \
+      "@com_google_googleapis//google/cloud/secretmanager/v1:secretmanager_proto" \
+      "@com_google_googleapis//google/cloud/secretmanager/logging/v1:logging_proto"
+  )"
   ["spanner"]="$(
     printf ",%s" \
       "@com_google_googleapis//google/spanner/v1:spanner_proto" \
```

You can find out which proto rules exist in the relevant directory using:

```shell
bazel --batch query --noshow_progress --noshow_loading_progress \
    "kind(cc_library, @com_google_googleapis//${path}/...)" | \
    grep -v cc_proto
```

## Update the Generator Configuration

Determine the retryable status codes by looking in the service config JSON. For
example, [here][retryable-status-codes].

[retryable-status-codes]: https://github.com/googleapis/googleapis/blob/0fea253787a4f2769b97b0ed3a8f5b28ef17ffa7/google/cloud/secretmanager/v1/secretmanager_grpc_service_config.json#L77-L80

Manually edit `generator/generator_config.textproto` and add the new service.
For example:

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

## Commit these changes

Create your first commit with purely hand-crafted changes

```shell
cd $HOME/google-cloud-cpp
git checkout -b feat-${library}-generate-library # Don't forget
git commit -m"feat(${library}): generate library" external/ generator/
```

## Update the list of proto files and proto dependencies

```shell
external/googleapis/update_libraries.sh
```

## Run the Scaffold Generator

Then run the micro-generator to create the scaffold and the C++ sources:

```shell
cd $HOME/google-cloud-cpp
mkdir -p "google/cloud/${library}"
bazel_output_base="$(bazel info output_base)"
bazel run \
  //generator:google-cloud-cpp-codegen -- \
  --protobuf_proto_path="${bazel_output_base}"/external/com_google_protobuf/src \
  --googleapis_proto_path="${bazel_output_base}"/external/com_google_googleapis \
  --output_path="${PWD}" \
  --config_file="${PWD}/generator/generator_config.textproto" \
  --scaffold="google/cloud/${library}"
```

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
streaming `*Updater` function. Use `google/cloud/bigquery/streaming.cc` for
inspiration.

## Potentially fix the bazel build

The generated `BUILD.bazel` file may require manual editing. The scaffold will
add one dependency from `@com_github_googleapis//${path}`, which might not be
correct. You may need to modify that dependency and/or add additional
dependencies for more complex libraries.

## Update the quickstart

The generated quickstart will need some editing. Use a simple operation, maybe
an admin operation listing top-level resources, to demonstrate how to use the
API.

## Update the README files

The following files probably need some light copy-editing to read less like they
were written by a robot:

- `google/cloud/${library}/README.md`
- `google/cloud/${library}/quickstart/README.md`
- `google/cloud/${library}/doc/main.dox`

The Cloud documentation links (`cloud.google.com/*/docs/*`) in these files are
not always valid. Find the correct urls and update the links.

## Update the root files

Manually edit `BUILD.bazel` to reference the new targets in
`//google/cloud/${library}`. Initially prefix your targets with
`:experimental-`.

Manually edit `.bazelignore` to include the newest
`google/cloud/${library}/quickstart/` directory. The ubsan build fails
otherwise.

Manually edit `.codecov.yml` to exclude generated code from the code coverage
reports.

## Fix formatting nits

```shell
ci/cloudbuild/build.sh -t checkers-pr
```

> :warning: This command might need to be run twice to format the quickstart
> code within the README.

## Verify everything compiles

```shell
ci/cloudbuild/build.sh -t cmake-install-pr
ci/cloudbuild/build.sh -t bazel-targets-pr
```

It is somewhat common for the build to fail due to expected install directories.
If this happens, make an edit to `cmake-install.sh`. For example:

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

## Commit these changes

```shell
git commit -m"Manually update READMEs, quickstart, and top-level stuff" \
  "google/cloud/${library}" \
  BUILD.bazel .codecov.yml .bazelignore
```
