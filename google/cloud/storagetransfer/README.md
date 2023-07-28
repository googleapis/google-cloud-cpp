# Storage Transfer API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Storage Transfer API][cloud-service-docs], a service to transfer data from
external data sources to a Google Cloud Storage bucket or between Google Cloud
Storage buckets.

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
#include "google/cloud/storagetransfer/v1/storage_transfer_client.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace storagetransfer = ::google::cloud::storagetransfer_v1;
  auto client = storagetransfer::StorageTransferServiceClient(
      storagetransfer::MakeStorageTransferServiceConnection());

  ::google::storagetransfer::v1::ListTransferJobsRequest request;
  request.set_filter(R"""({"projectId": ")""" + std::string{argv[1]} + "\"}");
  for (auto r : client.ListTransferJobs(request)) {
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

- Official documentation about the [Storage Transfer API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/storage-transfer
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/storagetransfer/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/storagetransfer
