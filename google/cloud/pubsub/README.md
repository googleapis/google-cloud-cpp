# Google Cloud Pub/Sub C++ Client Library

This directory contains an idiomatic C++ client library for interacting with
[Cloud Pub/Sub](https://cloud.google.com/pubsub/), an asynchronous messaging
service that decouples services that produce events from services that process
events.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/pubsub/publisher.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <project-id> <topic-id>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const topic_id = argv[2];

  // Create a namespace alias to make the code easier to read.
  namespace pubsub = ::google::cloud::pubsub;

  auto publisher = pubsub::Publisher(
      pubsub::MakePublisherConnection(pubsub::Topic(project_id, topic_id)));
  auto id =
      publisher
          .Publish(pubsub::MessageBuilder{}.SetData("Hello World!").Build())
          .get();
  if (!id) throw std::move(id).status();
  std::cout << "Hello World published with id=" << *id << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Pub/Sub][cloud-pubsub-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-pubsub-docs]: https://cloud.google.com/pubsub/docs/
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/pubsub/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/pubsub
