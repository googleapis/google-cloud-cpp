# Vector Search API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Vector Search API][cloud-service-docs].

The Vector Search API provides a fully-managed, highly performant, and scalable
vector database designed to power next-generation search, recommendation, and
generative AI applications. It allows you to store, index, and query your data
and its corresponding vector embeddings through a simple, intuitive interface.
With Vector Search, you can define custom schemas for your data, insert objects
with associated metadata, automatically generate embeddings from your data, and
perform fast approximate nearest neighbor (ANN) searches to find semantically
similar items at scale.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/vectorsearch/v1/vector_search_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace vectorsearch = ::google::cloud::vectorsearch_v1;
  auto client = vectorsearch::VectorSearchServiceClient(
      vectorsearch::MakeVectorSearchServiceConnection());

  for (auto r : client.ListCollections(location.FullName())) {
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

- Official documentation about the [Vector Search API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://docs.cloud.google.com/vertex-ai/docs/vector-search-2/overview
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/vectorsearch/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/vectorsearch
