# Adding new Libraries to `google-cloud-cpp`

This document describes the steps required to add a new library to
`google-cloud-cpp`. The document is intended for contributors to the
`google-cloud-cpp` libraries, it assumes you are familiar with the build systems
used in these libraries, that you are familiar with existing libraries, and with
which libraries are based on gRPC.

We will use `tasks` as an example through this document, you will need to
update the instructions based on whatever library you are adding.

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

You can find out which proto rules exist in the relevant directory using something like:

```shell
bazel --batch query --noshow_progress --noshow_loading_progress \
    "kind(cc_library, @com_google_googleapis//${path}/...)" | \
    grep -v cc_proto
```



## Update the list of proto files and proto dependencies

```shell
cd $HOME/google-cloud-cpp
external/googleapis/update_libraries.sh
```

## Run the Scaffold Generator

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

## Fix formatting of existing libraries

```shell
ci/cloudbuild/build.sh -t checkers-pr
```

## Create any custom source files

If the `generator/generator_config.textproto` entry for the service does not
enumerate the `retryable_status_codes`, you need to manually create the file as
`google/cloud/$library/internal/<service_name>_retry_traits.h`.

Likewise, for services using streaming operations you may need to implement the
streaming `*Updater` function. Use `google/cloud/bigquery/streaming.cc` for
inspiration.

## Validate and Generate the `*.bzl` files

Run the `cmake-install-pr` build.  This is one of the builds that compiles all
the libraries, including the library you just added. It will also generate the
`*.bzl` files for Bazel-based builds.

```shell
ci/cloudbuild/build.sh -t cmake-install-pr
```

## Update the README file

The generated `README.md` file in `google/cloud/${library}` probably needs some
light copy-editing to read less like it was written by a robot.

## Update the root `BUILD` file

Manually edit `BUILD` to reference the new targets in
`//google/cloud/tasks`. Initially prefix your targets with
`:experimental-`.

## Fix formatting nits

```shell
ci/cloudbuild/build.sh -t checkers-pr
```

## Add the libraries to git

```shell
git add \
  generator/generator_config.textproto \
  ci/etc/ \
  external/googleapis \
  google/cloud/${library}
```
