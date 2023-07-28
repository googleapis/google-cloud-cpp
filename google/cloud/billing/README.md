# Cloud Billing Budget API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Billing API][cloud-service-docs], a service that interacts with billing
accounts, billing budgets, and billing catalogs.

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
#include "google/cloud/billing/v1/cloud_billing_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << "\n";
    return 1;
  }

  namespace billing = ::google::cloud::billing_v1;
  auto client =
      billing::CloudBillingClient(billing::MakeCloudBillingConnection());

  for (auto a : client.ListBillingAccounts()) {
    if (!a) throw std::move(a).status();
    std::cout << a->DebugString() << "\n";
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
  [Cloud Billing Budget API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/billing
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/billing/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/billing
