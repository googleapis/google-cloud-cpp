# Adding new Libraries to `google-cloud-cpp`

This document describes the steps required to add a new library to
`google-cloud-cpp`. The document is intended for contributors to the
`google-cloud-cpp` libraries, it assumes you are familiar with the build systems
used in these libraries, that you are familiar with existing libraries, and with
which libraries are based on gRPC.

We will use `tasks` as an example through this document, you will need to
update the instructions based on whatever library you are adding.

## Set useful variables

```shell
cd $HOME/google-cloud-cpp
rule="google/cloud/tasks/v2:tasks_proto"
path="${rule%:*}"
library="tasks"
mkdir -p google/cloud/${library}
```

## Verify the C++ rules exist

```shell
bazel --batch query --noshow_progress --noshow_loading_progress \
    "deps(@com_google_googleapis//${rule/_proto/_cc_grpc})"
```

If this fails, send a CL to add the rule. Wait until that is submitted and
exported before proceeding any further.

## Update the list of proto files

CMake needs to know which proto files are created by downloading the
`googleapis` tarball. A bit annoying, but easy to generate:

```shell
cd $HOME/google-cloud-cpp
bazel --batch query --noshow_progress --noshow_loading_progress \
  "deps(@com_google_googleapis//${rule})" \
  | grep "@com_google_googleapis//${path}" \
  | grep -E '\.proto$' \
  >external/googleapis/protolists/${library}.list
```

## Update the list of proto dependencies

```shell
cd $HOME/google-cloud-cpp
bazel --batch query --noshow_progress --noshow_loading_progress \
  "deps(@com_google_googleapis//${rule})" \
  | grep "@com_google_googleapis//" | grep _proto \
  | grep -v "@com_google_googleapis//${path}" \
  >external/googleapis/protodeps/${library}.deps
```

## Run the Scaffold Generator

Manually edit `generator/generator_config.textproto` and add the new service.
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

## Manually add the C++ files to the CMakeLists.txt file

Create the `google/cloud/$library/internal/retry_traits.h` file, and add this file
**and** any generated C++ files to the `CMakeLists.txt` file. There should be
markers in the file, search for `EDIT HERE`.

```shell
ci/cloudbuild/build.sh -t cmake-install-pr
```

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
