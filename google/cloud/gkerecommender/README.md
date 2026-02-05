# GKE Recommender API C++ Client Library

This directory contains an idiomatic C++ client library for the
[GKE Recommender API][cloud-service-docs].

GKE Recommender API

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/gkerecommender/v1/gke_inference_quickstart_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << "\n";
    return 1;
  }

  namespace gkerecommender = ::google::cloud::gkerecommender_v1;
  auto client = gkerecommender::GkeInferenceQuickstartClient(
      gkerecommender::MakeGkeInferenceQuickstartConnection());
  for (auto r : client.FetchModels({})) {
    if (!r) throw std::move(r).status();
    std::cout << *r << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [GKE Recommender API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/kubernetes-engine/docs/how-to/machine-learning/inference-quickstart
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/gkerecommender/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/gkerecommender
