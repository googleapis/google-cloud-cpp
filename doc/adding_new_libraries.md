# Adding new Libraries to `google-cloud-cpp`

This document describes the steps required to add a new library to
`google-cloud-cpp`. The document is intended for contributors to the
`google-cloud-cpp` libraries, it assumes you are familiar with the build systems
used in these libraries, that you are familiar with existing libraries, and with
which libraries are based on gRPC.

We will use `pubsublite` as an example through this document, you will need to
update the instructions based on whatever library you are adding.

## Update the list of proto files

CMake needs to know which proto files are created by downloading the
`googleapis` tarball. A bit annoying, but easy to generate:

```shell
cd $HOME/google-cloud-cpp
bazel query \
  "deps(@com_google_googleapis//google/cloud/pubsublite/v1:pubsublite_proto)" \
  | grep "@com_google_googleapis//google/cloud/pubsublite/v1" \
  | grep -E '\.proto$' \
  >external/googleapis/protolists/pubsublite.list
```

## Update the list of proto dependencies

```shell
cd $HOME/google-cloud-cpp
bazel query \
  "deps(@com_google_googleapis//google/cloud/pubsublite/v1:pubsublite_proto)" \
  | grep "@com_google_googleapis//" | grep _proto \
  | grep -v @com_google_googleapis//google/cloud/pubsublite/v1 \
  >external/googleapis/protodeps/pubsublite.deps
```

## Run the Scaffold Generator

Manually edit `generator/generator_config.textproto` and add your new service.

```shell
bazel_output_base="$(bazel info output_base)"
bazel run \
  //generator:google-cloud-cpp-codegen -- \
  --protobuf_proto_path="${bazel_output_base}"/external/com_google_protobuf/src \
  --googleapis_proto_path="${bazel_output_base}"/external/com_google_googleapis \
  --output_path="${PWD}" \
  --config_file="${PWD}/generator/generator_config.textproto" \
  --scaffold="google/cloud/pubsublite"
```

## Manually add the C++ files to the CMakeLists.txt file

Create the `google/cloud/$library/retry_traits.h` file, and add this file and
any generated C++ files to the `CMakeLists.txt` file. There should be markers
in the file, search for `EDIT HERE`.

## Run the CI build to generate Bazel files

```shell
ci/cloudbuild/build.sh -t cmake-install-pr
```

## Update the root `BUILD` file

Manually edit `BUILD` to reference the new targets in
`//google/cloud/pubsublite`. Initially prefix your targets with
`:experimental-`.

## Fix formatting nits

```shell
ci/cloudbuild/build.sh -t checkers-pr
```
