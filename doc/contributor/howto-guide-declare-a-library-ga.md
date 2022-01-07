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

## Updating READMEs

Verify that the following documents describe the library as GA, or at least do
not describe the library as experimental:

- `README.md`: link the new library README files from the list of GA libraries.
- `CHANGELOG.md`: update the release notes for the next release announcing the
  new libraries.
- `google/cloud/${library}/README.md`: remove the comments stating that the
  library is experimental. Change them to say that the library is GA, and that
  `google-cloud-cpp` does not follow semantic versioning.
- `google/cloud/${library}/doc/main.dox`: ditto.
- `google/cloud/${library}/quickstart/README.md`: hopefully no changes.
- `google/cloud/${library}/CMakeLists.txt` sets the `DOXYGEN_PROJECT_NUMBER`
  to `${PROJECT_VERSION} (Experimental)`. Change that to `${PROJECT_VERSION}`.

## Update the targets and rules

Update the top-level `BUILD.bazel` file and **add** rules for `//:${library}`
and `//:${library}_mocks` without the `experimental-` prefix. You should
**NOT** remove the existing targets with the `experimental-` prefix. These
targets are used in the quickstart, and we link the quickstart against the
**previous** release of `google-cloud-cpp`.  Feel free to mark them as
deprecated.

Update the CMake targets in `google/cloud/${library}/CMakeLists.txt` and remove
any existing `experimental-` prefixes. Then add alias targets in the
`config.cmake.in` file, for example:

```patch
diff --git a/google/cloud/secretmanager/config.cmake.in b/google/cloud/secretmanager/config.cmake.in
index ad00ed7eb..996572a0c 100644
--- a/google/cloud/secretmanager/config.cmake.in
+++ b/google/cloud/secretmanager/config.cmake.in
@@ -20,3 +20,8 @@ find_dependency(google_cloud_cpp_grpc_utils)
 find_dependency(absl)

 include("${CMAKE_CURRENT_LIST_DIR}/google_cloud_cpp_secretmanager-targets.cmake")
+
+if (NOT TARGET google-cloud-cpp::experimental-secretmanager)
+    add_library(google-cloud-cpp::experimental-secretmanager ALIAS
+                google-cloud-cpp::secretmanager)
+endif ()
```

## Remove the `experimental-` rules and targets

Once a release is created *and* the release is included in `vcpkg`, change the
quickstart guide to reference the rules and targets without an `experimental-`
prefix. Then you can remove these rules and targets.
