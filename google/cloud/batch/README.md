# Batch API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Batch API][cloud-service-docs], a fully managed service to schedule, queue, and
execute batch jobs on Google's infrastructure.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

For detailed instructions on how to build and install this library, see the
top-level [README](/README.md#building-and-installing).

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/batch/v1/batch_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id region-id\n";
    return 1;
  }

  namespace batch = ::google::cloud::batch_v1;
  auto client = batch::BatchServiceClient(batch::MakeBatchServiceConnection());

  auto const location =
      std::string{"projects/"} + argv[1] + "/locations/" + argv[2];
  for (auto r : client.ListJobs(location)) {
    if (!r) throw std::move(r).status();
    std::cout << r->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Batch API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/batch
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-batch/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/batch
