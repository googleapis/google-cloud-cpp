# Google Cloud C++ Client Libraries

[![Documentation][doxygen-shield]][doxygen-link]

[doxygen-shield]: https://img.shields.io/badge/documentation-master-brightgreen.svg
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-common/latest/

This directory contains the following C++ client libraries:

* [Cloud Bigtable](bigtable/README.md)
* [Google Cloud Storage](storage/README.md)

In addition some utilities shared across these libraries are also found here.
Most of these utilities are in the `google::cloud::internal` namespace, and are
not intended for direct use by consumers of the C++ client libraries.

## Documentation

Documentation for the common utilities is available [online][doxygen-link].

## Release Notes

### v0.13.x - TBD

### v0.12.x - 2019-10

* bug: fix runtime install directory (#3063)

### v0.11.x - 2019-09

* feat: Use macros for compiler id and version (#2937)
* fix: Fix bazel build on windows. (#2940)
* chore: Keep release tags in master branch. (#2963)
* cleanup: Use only find_package to find dependencies. (#2967)
* feat: Add ability to disable building libraries (#3025)
* bug: fix builds with CMake 3.15 (#3033)
* feat: Document behavior of passing empty string to SetEnv on Windows (#3030)

### v0.10.x - 2019-08

* feat: add `conjunction` metafunction (#2892)
* bug: Fix typo in testing_util/config.cmake.in (#2851)
* bug: Include 'IncludeGMock.cmake' in testing_util/config.cmake.in (#2848)

### v0.9.x - 2019-07

* feature: support `operator==` and `operator!=` for `StatusOr`.
* feature: support assignment from `Status` in `StatusOr`.
* feature: disable `optional<T>`'s converting constructor when
  `U == optional<T>`.

### v0.7.x - 2019-05

* Support move-only callables in `future<T>`
* Avoid `std::make_exception_ptr()` in `future_shared_state_base::abandon()`.

### v0.6.x - 2019-04

* Avoid `std::make_exception_ptr()` when building without exceptions.
* Removed the googleapis submodule. The build system now automatically
  downloads all deps.

### v0.5.x - 2019-03

* **Breaking change**: Make `google::cloud::optional::operator bool()` explicit.
* Add `google::cloud::optional` value conversions that match `std::optional`.
* Stop using grpc's `DO_NOT_USE` enum.
* Remove ciso646 includes to force traditional spellings.
* Change `std::endl` -> `"\n"`.
* Enforce formatting of `.cc` files.

### v0.4.x - 2019-02

* Fixed some documentation.
* **Breaking change**: Removed `StatusOr<void>`.
* Updated `StatusOr` documentation.
* Fixed some (minor) issues found by coverity.

### v0.3.x - 2019-01

* Support compiling with gcc-4.8.
* Fix `GCP_LOG()` macro so it works on platforms that define a `DEBUG`
  pre-processor symbol.
* Use different PRNG sequences for each backoff instance, previously all the
  clones of a backoff policy shared the same sequence.
* Workaround build problems with Xcode 7.3.

### v0.2.x - 2018-12

* Implement `google::cloud::future<T>` and `google::cloud::promise<T>` based on
  ISO/IEC TS 19571:2016, the "C++ Extensions for Concurrency" technical
  specification, also known as "futures with continuations".

### v0.1.x - 2018-11

* `google::cloud::optional<T>` an intentionally incomplete implementation of
  `std::optional<T>` to support C++11 and C++14 users.
* Applications can configure `google::cloud::LogSink` to enable logging in some
  of the libraries and to redirect the logs to their preferred destination.
  The libraries do not enable any logging by default, not even to `stderr`.
* `google::cloud::SetTerminateHandler()` allows applications compiled without
  exceptions, but using the APIs that rely on exceptions to report errors, to
  configure how the application terminates when an unrecoverable error is
  detected by the libraries.
