# Cloud Profiler API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Profiler API][cloud-service], a service that manages continuous CPU and
heap profiling to improve performance and reduce costs.

Note that this library allows you to interact with Cloud Profiler, but Cloud
Profiler does not yet offer profiling of C++ applications. The
[types of profiling available][profiling] are listed in the service
documentation.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

For detailed instructions on how to build and install this library, see the
top-level [README](/README.md#building-and-installing).

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/profiler/v2/profiler_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace profiler = ::google::cloud::profiler_v2;
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

## More Information

- Official documentation about the [Cloud Profiler API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service]: https://cloud.google.com/profiler
[cloud-service-docs]: https://cloud.google.com/profiler/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/profiler/latest/
[profiling]: https://cloud.google.com/profiler/docs/concepts-profiling#types_of_profiling_available
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/profiler
