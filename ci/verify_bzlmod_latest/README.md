# Verify Bazel targets with latest module dependency versions

This workspace verifies that `google_cloud_cpp` targets build and pass tests
when used as a dependency in a Bzlmod project that specifies the latest released
versions of core dependencies (such as Abseil, Protobuf, gRPC, OpenTelemetry,
etc.).

Under Bzlmod's Minimal Version Selection (MVS), specifying higher dependency
versions in this root module overrides the minimum versions declared in the root
`//MODULE.bazel` file.
