# Service Control API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Service Control API][cloud-service-docs]. This service provides admission
control and telemetry reporting for services integrated with Service
Infrastructure.

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
#include "google/cloud/servicecontrol/v1/service_controller_client.h"
#include "google/cloud/project.h"
#include "google/protobuf/util/time_util.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace servicecontrol = ::google::cloud::servicecontrol_v1;
  auto client = servicecontrol::ServiceControllerClient(
      servicecontrol::MakeServiceControllerConnection());

  auto const project = google::cloud::Project(argv[1]);
  google::api::servicecontrol::v1::CheckRequest request;
  request.set_service_name("pubsub.googleapis.com");
  *request.mutable_operation() = [&] {
    using ::google::protobuf::util::TimeUtil;

    google::api::servicecontrol::v1::Operation op;
    op.set_operation_id("TODO-use-UUID-4-or-UUID-5");
    op.set_operation_name("google.pubsub.v1.Publisher.Publish");
    op.set_consumer_id(project.FullName());
    *op.mutable_start_time() = (TimeUtil::GetCurrentTime)();
    return op;
  }();

  auto response = client.Check(request);
  if (!response) throw std::move(response).status();
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Service Control API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/service-control
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/servicecontrol/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/servicecontrol
