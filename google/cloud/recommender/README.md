# Recommender C++ Client Library

This directory contains an idiomatic C++ client library for
[Recommender][cloud-service], a service on Google Cloud that provides usage
recommendations and insights for Cloud products and services.

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
#include "google/cloud/recommender/v1/recommender_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace recommender = ::google::cloud::recommender_v1;
  auto client =
      recommender::RecommenderClient(recommender::MakeRecommenderConnection());
  // For additional recommenders see:
  //     https://cloud.google.com/recommender/docs/recommenders#recommenders
  auto const parent =
      location.FullName() +
      "/recommenders/google.compute.instance.MachineTypeRecommender";
  for (auto r : client.ListRecommendations(parent)) {
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

- Official documentation about the [Recommender][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service]: https://cloud.google.com/recommender
[cloud-service-docs]: https://cloud.google.com/recommender/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/recommender/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/recommender
