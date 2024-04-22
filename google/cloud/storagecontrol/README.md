# Storage Control API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Storage Control API][cloud-service-docs], a service to manage GCS
configuration.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/storagecontrol/v2/storage_control_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " bucket-id\n";
    return 1;
  }
  std::string const bucket_name = std::string{"projects/_/buckets/"} + argv[1];

  namespace storagecontrol = ::google::cloud::storagecontrol_v2;
  auto client = storagecontrol::StorageControlClient(
      storagecontrol::MakeStorageControlConnection());

  for (auto r : client.ListFolders(bucket_name)) {
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

- Official documentation about the [Storage Control API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/storage/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/storagecontrol/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/storagecontrol
