# Cloud Profiler API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Profiler API][cloud-service], a service that manages continuous CPU
and heap profiling to improve performance and reduce costs.

Note that this library allows you to interact with Cloud Profiler, but Cloud
Profiler does not yet offer profiling of C++ applications. The
[types of profiling available][profiling] are listed in the service
documentation.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Supported Platforms

- Windows, macOS, Linux
- C++14 (and higher) compilers (we test with GCC >= 7.3, Clang >= 6.0, and
  MSVC >= 2017)
- Environments with or without exceptions
- Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

- Official documentation about the [Cloud Profiler API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/profiler/profiler_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace profiler = ::google::cloud::profiler;
  auto client = profiler::ProfilerServiceClient(
      profiler::MakeProfilerServiceConnection());

  google::devtools::cloudprofiler::v2::CreateProfileRequest req;
  req.set_parent(google::cloud::Project(argv[1]).FullName());
  req.add_profile_type(google::devtools::cloudprofiler::v2::CPU);
  auto& deployment = *req.mutable_deployment();
  deployment.set_project_id(argv[1]);
  deployment.set_target("quickstart");

  auto profile = client.CreateProfile(req);
  if (!profile) throw std::move(profile).status();
  std::cout << profile->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

- Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
- Developers that prefer using a package manager such as
  [vcpkg](https://vcpkg.io), [Conda](https://conda.io),
  or [Conan](https://conan.io) should follow the instructions for their package
  manager.
- Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
- Developers wanting to compile the library just to run some examples or
  tests should read the current document.
- Contributors and developers to `google-cloud-cpp` should consult the guide to
  [setup a development workstation][howto-setup-dev-workstation].

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.

[cloud-service]: https://cloud.google.com/profiler
[cloud-service-docs]: https://cloud.google.com/profiler/docs
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-profiler/latest/
[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md
[profiling]: https://cloud.google.com/profiler/docs/concepts-profiling#types_of_profiling_available
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/profiler
