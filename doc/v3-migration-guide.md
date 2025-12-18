# `google-cloud-cpp` v3 Migration Guide

This document is intended for users of previous major versions (v1.x.y, v2.x.y)
of the `google-cloud-cpp` SDK who are moving to a release on the v3.x.y series.

While this repository does not strictly follow semver, it does use the major
version number to indicate large breaking changes. We strive to balance the
frequency in which we introduce breaking changes with improvements to the SDK.
Since our most recent major version increment, about 3 years ago, we have added
new API surfaces that supersede the previous deprecated types and functions. As
part of the v3 release series, we are decommissioning those deprecated API
surfaces. This guide serves a central location to document how to migrate from
the decommissioned API surfaces to their replacements.

## C++17

Depending on your build system of choice, you should set the appropriate flag
for your compiler if it does not already default to `--std=c++17` or higher.

## Bazel Central Registry

Bazel is moving away from WORKSPACE file support to using modules from the Bazel
Central Registry. Part of the v3.x.y release series includes supporting the new
[google-cloud-cpp](https://registry.bazel.build/modules/google_cloud_cpp) Bazel
module which can be added to your `MODULE.bazel` file as a dependency.

## Dependencies

### Previously Optional Dependencies that are now Required

- `libcurl`
- `nlohmann_json`
- `opentelemetry-cpp`

### Relocated Dependencies

- `crc32c`

## Decommissioned API Surfaces

### Bazel

### CMake

### Common

### Bigtable

### Pubsub

### Spanner

### Storage
