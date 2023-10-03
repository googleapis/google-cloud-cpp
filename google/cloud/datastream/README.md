# Datastream API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Datastream API][cloud-service-docs], a serverless and easy-to-use change data
capture (CDC) and replication service. It allows you to synchronize data across
heterogeneous databases and applications reliably, and with minimal latency and
downtime.

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
#include "google/cloud/datastream/v1/datastream_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace datastream = ::google::cloud::datastream_v1;
  auto client =
      datastream::DatastreamClient(datastream::MakeDatastreamConnection());

  for (auto s : client.ListStreams(location.FullName())) {
    if (!s) throw std::move(s).status();
    std::cout << s->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Datastream API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/datastream
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/datastream/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/datastream
