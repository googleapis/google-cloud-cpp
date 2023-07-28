# Cloud Commerce Consumer Procurement API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Commerce Consumer Procurement API][cloud-service-docs], a service that
enables consumers to procure products served by Cloud Marketplace platform.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/commerce/consumer/procurement/v1/consumer_procurement_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " billing-account\n";
    return 1;
  }

  namespace commerce = ::google::cloud::commerce_consumer_procurement_v1;
  auto client = commerce::ConsumerProcurementServiceClient(
      commerce::MakeConsumerProcurementServiceConnection());

  auto const parent = std::string("billingAccounts/") + argv[1];
  for (auto r : client.ListOrders(parent)) {
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

- Official documentation about the
  [Cloud Commerce Consumer Procurement API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/commerce
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/commerce/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/commerce
