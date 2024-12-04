# Enabling ctype=CORD workarounds for older Protobuf versions

The `[ctype = CORD]` annotation in a proto file changes the C++ representation
of `bytes` and `string` fields from `std::string` to `absl::Cord`. This
annotation is critical to get good performance with GCS and gRPC.

Protobuf does not support `[ctype = CORD]` until 4.23.x (aka v23.x). In older
versions the protobuf compiler "hides" the accessors and modifiers for fields
with this annotation. The accessors and modifiers become `private` in the
generated C++ classes.

We can use some workarounds to access these private member functions, but these
workarounds do not compile when Protobuf supports `[ctype = CORD]`.

We cannot require Protobuf >= v23 until the 3.x release of `google-cloud-cpp`,
as that would be a breaking change. We need to support compilation with or
without support for `[ctype = CORD]`.

We need to disable the workarounds using a `#if` / `#endif` block. Protobuf does
not provide any pre-processor macros to detect if it supports `[ctype = CORD]`.
We need to provide our own C++ pre-processor macros for this purpose.

While we can try to chose a good default for these macros, these defaults will
be incorrect in specific environments.

The `google-cloud-cpp` code uses a `static_assert()` to detect if the macros are
set incorrectly, and prints an error message pointing to this document.

## Enabling ctype=CORD workarounds with CMake

If you are using CMake as your build system the
`GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND` CMake option enables the
workaround. When compiling with older versions of Protobuf add
`-DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND=ON` to your CMake configuration
line. For example:

```
cmake -S . -B ... -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND=ON
```

Use `-DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND=OFF` with Protobuf >= v23:

```
cmake -S . -B ... -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND=OFF
```

If you are not building `google-cloud-cpp` from source, then you are probably
using a package manager, such as [vcpkg](https://vcpkg.io) or
[Conda](https://conda.io). The `google-cloud-cpp` recipes in the package manager
should match the Protobuf version in that package manager. If their
configuration is incorrect, please file a bug with the package maintainers.

## Enabling ctype=CORD workarounds with Bazel

If you are using Bazel as your build system the
`//:enable-ctype-cord-workaround` Bazel flag enables the workaround. This flag
must be used with whatever name *you* gave to the `google-cloud-cpp` repository
in your `WORKSPACE` file. Set this flag to `true` if using older versions of
Protobuf. For example:

```
bazel build --@google_cloud_cpp//:enable-ctype-cord-workaround=true ...
```

Set this flag to `false` if using Protobuf >= v23. For example:

```
bazel build --@google_cloud_cpp//:enable-ctype-cord-workaround=false ...
```

## Defaults during the transition period

As of 2023-04-10 the proto files for the Google Cloud Storage service do not
contain any `[ctype = CORD]` annotations. The default will be to disable the
workarounds until this annotation is added. As long as the annotation is not
present, this is the correct default with **any** Protobuf version.

Once the `[ctype = CORD]` annotation is included in the proto files then the
correct default depends on the Protobuf version. If `google-cloud-cpp` is using
Protobuf < v23 and the annotation is included in the proto files, then
`google-cloud-cpp` will enable the workarounds by default.

It is very likely that the default value for this option is incorrect during the
transition period. This is cumbersome, but unavoidable until Protobuf >= v23 is
widely deployed.
