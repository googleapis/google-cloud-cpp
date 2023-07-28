# Cloud Channel API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Channel API][cloud-service-docs], a service that enables Google Cloud
partners to have a single unified resale platform and APIs across all of Google
Cloud including GCP, Workspace, Maps and Chrome.

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
#include "google/cloud/channel/v1/cloud_channel_client.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " account-id\n";
    return 1;
  }

  namespace channel = ::google::cloud::channel_v1;
  auto client = channel::CloudChannelServiceClient(
      channel::MakeCloudChannelServiceConnection());

  // Fill in this request as needed.
  auto request = google::cloud::channel::v1::ListProductsRequest{};
  request.set_account(std::string("accounts/") + argv[1]);
  for (auto r : client.ListProducts(std::move(request))) {
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

- Official documentation about the [Cloud Channel API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/channel
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/channel/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/channel
