# Game Services API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Game Services API][cloud-service-docs], a service to deploy and manage
infrastructure for global multiplayer gaming experiences.

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
#include "google/cloud/gameservices/v1/game_server_clusters_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id realm-id\n";
    return 1;
  }

  namespace gameservices = ::google::cloud::gameservices_v1;
  auto client = gameservices::GameServerClustersServiceClient(
      gameservices::MakeGameServerClustersServiceConnection());

  auto const realm = "projects/" + std::string(argv[1]) + "/locations/" +
                     std::string(argv[2]) + "/realms/" + std::string(argv[3]);
  for (auto r : client.ListGameServerClusters(realm)) {
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

- Official documentation about the [Game Services API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/game-servers/docs/
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-gameservices/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/gameservices
