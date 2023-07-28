# The `google-cloud-cpp` public API

This document describes what constitutes `google-cloud-cpp`'s public API.

As this project follows Google's [OSS Library Breaking Change Policy], any
breaking changes in the public API require increasing the major version number
in the library.

While we take commercially reasonable efforts to prevent breaks in the public
API, it is possible that backwards incompatible changes go undetected and,
therefore, undocumented. We apologize if this is the case and welcome feedback
or bug reports to rectify the problem.

We make no guarantees and make no effort to maintain compatibility for any
functions, classes, libraries, files, targets or any other artifact that is not
**explicitly** included in the public API.

Note that this document has no bearing on the Google Cloud Platform deprecation
policy described at https://cloud.google.com/terms.

Previous versions of the library will remain available on the
[GitHub Releases page]. In many cases, you will be able to use an older version
even if a newer version has changes that you are unable (or do not have time) to
adopt.

We think this document covers all interface points. If we missed something
please file a [GitHub issue][github-issue].

## Public API Overview

The public API includes:

- Any C++ names, including macros, classes, structs, typedefs, functions, enums,
  etc. in public namespaces.
- Public namespaces are of the form:
  - `google::cloud`
  - `google::cloud::mocks`
  - `google::cloud::${library}` and `google::cloud::${library}_mocks` where
    `${library}` matches the regular expression `^[a-z][a-z_]*$` and
    `${library}` is **not** `internal`.
  - `google::cloud::${library}_v${number}` and
    `google::cloud::${library}_v${number}_mocks` where `${library}` matches
    `^[a-z][a-z]*$` and `${number}` matches `^[0-9][0-9]*$`. In the common case
    `${number}` is a single digit..
- C++ header file names in `google/cloud/*.h` or `google/cloud/${library}/*.h`,
  and `google/cloud/${library}/v${number}`, where `${library}` matches the
  regular expression `^[a-z][a-z_]*$` (but it is **not** `internal`), and
  `${number}` matches `^[0-9][0-9]*`.
- CMake targets with the `google-cloud-cpp::` prefix.
- Bazel targets in the top-level directory (e.g. `//:storage`).
- pkg-config modules starting with `google_cloud_cpp_`.
- Environment variables defined in the documentation.

The public API excludes:

- Any C++ symbol or file that includes `internal`, `impl`, `test`, `detail`,
  `benchmark`, `sample`, or `example` in its fully qualified name or path.
- Any CMake target that includes `internal` or `experimental` in its name.
- Any Bazel target that includes `internal` or `experimental` in its name.
- Environment variables with `EXPERIMENTAL` in their name.
- We are not talking about the gRPC or REST APIs exposed by Google Cloud
  servers. This is beyond the scope of what the client library can control.

We are also talking only about A**P**I stability -- the ABI is subject to change
without notice. You should not assume that binary artifacts (e.g. static
libraries, shared objects, dynamically loaded libraries, object files) created
with one version of the library are usable with newer/older versions of the
library. The ABI may, and does, change on "minor revisions".

## Guidelines

We request that our customers adhere to the following guidelines to avoid
accidentally depending on parts of the library we do not consider to be part of
the public API and therefore may change (including removal) without notice:

- You should only include headers matching the `google/cloud/${library}/*.h`,
  `google/cloud/${library}/mock/*.h` or `google/cloud/*.h` patterns.
- You should **NOT** directly include headers in any subdirectories, such as
  `google/cloud/${library}/internal`.
- The files *included from* our public headers are **not part of our public
  API**. Depending on indirect includes may break your build in the future, as
  we may change a header `"foo.h"` to stop including `"bar.h"` if `"foo.h"` no
  longer needs the symbols in `"bar.h"`. To avoid having your code broken, you
  should directly include the public headers that define all the symbols you use
  (this is sometimes known as [include what you use]).
- Any file or symbol that lives within a directory or namespace containing
  `internal`, `impl`, `test`, `detail`, `benchmark`, `sample`, or `example`, is
  explicitly **not part of our public API**.
- Any file or symbol with `Impl` or `impl` in its name is **not** part of our
  public API.
- Any symbol with `experimental` in its name is **not** part of the public API.
- You should avoid naming our inline namespaces (even avoid spelling the
  preprocessor names like `GOOGLE_CLOUD_CPP_NS`) and instead rely on them being
  a transparent versioning mechanism that you almost certainly don't care about.
  If you do spell out specific inline namespace names, your code will be tightly
  coupled with that specific version and will likely break when upgrading to a
  new version of our library.

## Beyond the C++ API

Applications developers interact with a C++ library through more than just the
C++ symbols and headers. They also need to reference the name of the library in
their build scripts. Depending on the build system they use this may be a CMake
target, a Bazel rule, a pkg-config module, or just the name of some object in
the file system.

As with the C++ API, we try to avoid breaking changes to these interface points.
We treat breaking changes to these interface points like any other breaking
change.

### Experimental Libraries

From time to time we add libraries to `google-cloud-cpp` to validate new
designs, expose experimental (or otherwise not generally available) GCP
features, or simply because a library is not yet complete. Such libraries will
have `experimental` in their CMake target and Bazel rule names. The README file
for these libraries will also document that they are experimental. Such
libraries are subject to change, including removal, without notice. This
includes, but it is not limited to, all their symbols, pre-processor macros,
files, targets, rules, and installed artifacts.

### Bazel rules

Only the rules exported at the top-level directory are intended for customer
use, e.g.,`//:spanner`. Experimental rules have `experimental` in their name,
e.g. `//:experimental-logging`. As previously stated, experimental rules are
subject to change or removal without notice.

### CMake targets and packages

Only CMake packages starting with the `google_cloud_cpp_` prefix are intended
for customer use. Only targets starting with `google-cloud-cpp::`, are intended
for customer use. Experimental targets have `experimental` in their name (e.g.
`google-cloud-cpp::experimental-logging`). As previously stated, experimental
targets are subject to change or removal without notice.

### pkg-config modules

Only modules starting with `google_cloud_cpp_` are intended for customer use.

### Unsupported use cases

We try to provide stable names for the previously described mechanisms:

- Bazel rules,
- CMake targets loaded via `find_package()`,
- pkg-config modules

It is certainly possible to use the library using other approaches. While these
may work, we may accidentally break these from time to time. Examples of such,
and the recommended alternatives, include:

- CMake's `FetchContent` and/or git submodules: in these approaches the
  `google-cloud-cpp` library becomes a subdirectory of a larger CMake build We
  do not test `google-cloud-cpp` in these configurations, and we find them
  brittle as **all** CMake targets become visible to the larger project. This is
  both prone to conflicts, and makes it impossible to enforce that some targets
  are only for testing or are implementation details. Applications may want to
  consider source package managers, such as `vcpkg`, or CMake super builds via
  `ExternalProject_Add()` as alternatives.

- Using library names directly: applications should not use the library names,
  e.g., by using `-lgoogle_cloud_cpp_bigtable` in build scripts. We may need to
  split or merge libraries over time, making such names unstable. Applications
  should use CMake targets, e.g., `google-cloud-cpp::bigtable`, or pkg-config
  modules, e.g., `$(pkg-config google_cloud_cpp_bigtable --libs)` instead.

### Environment Variables

Environment variables referenced in our documentation come with the same
guarantees as our public APIs. That is, we are committed to supporting their
behavior unless the behavior affects experimental components, or the environment
variable itself is experimental.

### Telemetry Data

The library can be configured to produce telemetry data, such as logs or traces.
We make no guarantees around the **contents** of such data. For example, we may
change the format of our logs, or the names of our spans without notice.

The APIs to programmatically enable logging and tracing have the same guarantees
as any other public APIs in our libraries.

### Documentation and Comments

The documentation (and its links) is intended for human consumption and not
third party websites, or automation (such as scripts scraping the contents). The
contents and links of our documentation may change without notice.

[github releases page]: https://github.com/googleapis/google-cloud-cpp/releases
[github-issue]: https://github.com/googleapis/google-cloud-cpp/issues/new/choose
[include what you use]: https://include-what-you-use.org/
[oss library breaking change policy]: https://opensource.google/documentation/policies/library-breaking-change
