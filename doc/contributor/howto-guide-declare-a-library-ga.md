# How-to Guide: Declare a library as GA

This document describes the steps required to promote a `google-cloud-cpp`
library to GA (General Availability). It is intended for contributors, and
assumes you are familiar with the build system used in the library.

This document applies to both hand-crafted and generated libraries, but mostly
it will be using generated libraries as examples.

## Overview

Declaring a library "GA" is largely a matter of updating the documentation and
the target names to indicate that the library is no longer experimental. You
typically need to take three steps, with an intermediate release before the last
step:

- Update the README files and Doxygen reference pages to indicate the library is
  now GA.
- Add new targets and rules for the library without the `experimental-` prefix.
- Remove the targets and rules with the `experimental-` prefix.

## Pre-requisites

Before we can declare a library GA we need to ensure it is of sufficient
quality. We largely follow the internal guidelines at (go/client-quality). For a
generated library there are 3 critical checks:

- The server API is GA: this can be non-trivial if you are developing a library
  while the service is still in private and/or public preview.
- The server functionality is fully exposed in the client surface: this can be
  non-trivial if the service uses streaming RPCs, as the generator does not
  fully support those at the moment.
- 28 days have elapsed since the pre-release of the major version: this means
  you need to wait 28 days after the latest release which included your library.

In addition, we (the Cloud C++ team) require a simple `quickstart.cc` for each
library. This program is typically created when the library is generated.

### `cmake/GoogleCloudCppFeatures.cmake`

Update `cmake/GoogleCloudCppFeatures.cmake`. Move the library from
`GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES` to
`GOOGLE_CLOUD_CPP_TRANSITION_LIBRARIES`. Do this first as it helps automate the
following steps.

```shell
mapfile -t ga < <(cmake -P cmake/print-ga-libraries.cmake 2>&1 |
  grep -E -v 'storage|bigtable|spanner|pubsub|common|grpc_utils')
```

### `CHANGELOG.md`

Update the release notes for the next release announcing the new libraries.

### `README.md`

Link the new library README files from the list of GA libraries. This is now
automated. As part of the `checkers-pr` script.

### `google/cloud/${library}/README.md`:

Remove the `:construction:` image

```shell
for lib in "${ga[@]}"; do sed -i '/:construction:/,+1d' google/cloud/${lib}/README.md; done
```

Change the comments stating that the library is experimental to state that the
library is GA, and that `google-cloud-cpp` does not follow semantic versioning.

```shell
for lib in "${ga[@]}"; do sed -i '/^This library is \*\*experimental/,+1d' google/cloud/${lib}/README.md; done
for lib in "${ga[@]}"; do sed -i 's/^Please note that the Google Cloud C/While this library is **GA**, please note that the Google Cloud C/' google/cloud/${lib}/README.md; done
```

### `google/cloud/${library}/doc/main.dox`:

Change the comments stating that the library is experimental to state that the
library is GA, and that `google-cloud-cpp` does not follow semantic versioning.

```shell
for lib in "${ga[@]}"; do sed -i '/^This library is \*\*experimental/,+1d' google/cloud/${lib}/doc/main.dox; done
for lib in "${ga[@]}"; do sed -i 's/^Please note that the Google Cloud C/While this library is **GA**, please note that the Google Cloud C/' google/cloud/${lib}/doc/main.dox; done
```

### `google/cloud/${library}/CMakeLists.txt`:

Update the CMake library targets and the quickstart runner.

```shell
for lib in "${ga[@]}"; do sed -i 's/EXPERIMENTAL/TRANSITION/' google/cloud/${lib}/CMakeLists.txt; done
for lib in "${ga[@]}"; do sed -i 's/experimental-//' google/cloud/${lib}/CMakeLists.txt; done
```

## Reference the GA targets in the quickstarts

For Bazel, we can drop the `experimental-` prefix as soon as renovate bot
updates our builds to use a release with the GA version of the library.

```shell
for lib in "${ga[@]}"; do sed -i 's/experimental-//' google/cloud/${lib}/quickstart/BUILD.bazel; done
```

For CMake, we can drop the `experimental-` prefix as soon as a release with the
GA version of the library is included in `vcpkg`.

```shell
for lib in "${ga[@]}"; do sed -i 's/experimental-//' google/cloud/${lib}/quickstart/CMakeLists.txt; done
```

## (Eventually) Remove the `experimental-` rules and targets

In the following release, move the libraries from
`GOOGLE_CLOUD_CPP_TRANSITION_LIBRARIES` to `GOOGLE_CLOUD_CPP_GA_LIBRARIES`, in
`cmake/GoogleCloudCppFeatures.cmake`.

Then remove the CMake aliases.

```shell
for lib in "${ga[@]}"; do sed -i 's/TRANSITION//' google/cloud/${lib}/CMakeLists.txt; done
```
