# How-to Guide: Declare a library as GA

This document describes the steps required to promote a `google-cloud-cpp`
library to GA. The document is intended for contributors to the
`google-cloud-cpp` libraries. It assumes you are familiar with the build systems
used in these libraries, and that you are familiar with existing libraries.

This document applies to both hand-crafted and generated libraries, but mostly
it will be using generated libraries as examples.

## Overview

Declaring a library "GA" is largely a matter of updating the documentation and
the target names to indicate that the library is no longer experimental. You
typically need to take three steps, with an intermediate release before the
last step:

- Update the README files and Doxygen reference pages to indicate the library is
  now GA.
- Add new targets and rules for the library without the `experimental-` prefix.
- Remove the targets and rules with the `experimental-` prefix.

## Pre-requisites

Before we can declare a library GA we need to ensure it is of sufficient
quality. We largely follow the internal guidelines at (go/client-quality).
For a generated library there are 3 critical checks:

- The server API is GA: this can be non-trivial if you are developing a library
  while the service is still in private and/or public preview.
- The server functionality is fully exposed in the client surface: this can be
  non-trivial if the service uses streaming RPCs, as the generator does not
  fully support those at the moment.
- 28 days have elapsed since the pre-release of the major version: this means
  you need to wait 28 days after the latest release which included your library.

In addition, we (the Cloud C++ team) require a simple `quickstart.cc` for each
library.  This program is typically created when the library is generated.

### `BUILD.bazel`

Update the top-level `BUILD.bazel` file. Move the library from
`EXPERIMENTAL_LIBRARIES` to `TRANSITION_LIBRARIES`. Do this first as it helps
automate the following steps.

```shell
mapfile -t ga < <(bazel --batch query \
  --noshow_progress --noshow_loading_progress \
  'kind(cc_library, //:all) except filter("experimental|mocks", kind(cc_library, //:all))' |
  sed -e 's;//:;;' | grep -E -v 'storage|bigtable|spanner|pubsub|common|grpc_utils')
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
for lib in "${ga[@]}"; do sed -i 's/^Please note that the Google Cloud C/While this library is **GA**, please note that the Google Cloud C/' google/cloud/${lib}/README.md; done
```

### `google/cloud/${library}/doc/main.dox`:

Change the comments stating that the library is experimental to state that the
library is GA, and that `google-cloud-cpp` does not follow semantic versioning.

```shell
for lib in "${ga[@]}"; do sed -i 's;^This library is \*\*experimental.*;While this library is **GA**, please note Google Cloud C++ client libraries do **not** follow [Semantic Versioning](https://semver.org/).;' google/cloud/${lib}/doc/main.dox; done
```

### `google/cloud/${library}/CMakeLists.txt`:

Change the definition of the `DOXYGEN_PROJECT_NUMBER` variable from
`${PROJECT_VERSION} (Experimental)` to `${PROJECT_VERSION}`.

```shell
for lib in "${ga[@]}"; do sed -i 's;"\${PROJECT_VERSION} (Experimental)";"${PROJECT_VERSION}";' google/cloud/${lib}/CMakeLists.txt; done
```

Change the target name from `google-cloud-cpp::experimental-${library}`:

```shell
for lib in "${ga[@]}"; do sed -i 's/google-cloud-cpp::experimental-/google-cloud-cpp::/' google/cloud/${lib}/CMakeLists.txt; done
```

### `google/cloud/${library}/config.cmake.in`:

Add an alias to help transition from `google-cloud-cpp::experimental-${library}`
to `google-cloud-cpp::${library}`:

```shell
for lib in "${ga[@]}"; do
  printf "\nif (NOT TARGET google-cloud-cpp::experimental-%s)\n    add_library(google-cloud-cpp::experimental-%s ALIAS google-cloud-cpp::%s)\nendif ()\n" "${lib}" "${lib}" "${lib}" >>google/cloud/${lib}/config.cmake.in
done
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

In the following release, move the libraries from `TRANSITION_LIBRARIES` to
`GA_LIBRARIES`, in the top-level `BUILD.bazel`.

Then remove the CMake aliases.

```shell
for lib in "${ga[@]}"; do sed -i '1,/-targets.cmake")/!d' google/cloud/${lib}/config.cmake.in; done
```
