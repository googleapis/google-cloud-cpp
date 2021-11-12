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

```shell
cmake \
  -DCOPYRIGHT_YEAR=2021 \
  -DNAME=pubsublite \
  -DSERVICE_NAME="Cloud Pub/Sub Lite" \
  -P generator/scaffold/create_scaffold.cmake
```

## Add the new library to the CI builds

Edit `ci/cloudbuild/builds/cmake-install.sh`.

## Run the CI build to generate Bazel files

```shell
ci/cloudbuild/build.sh -t cmake-install-pr
```
