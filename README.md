```cc
#include "google/cloud/storage/client.h"
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Missing bucket name.\n";
    std::cerr << "Usage: quickstart <bucket-name>\n";
    return 1;
  }
  std::string const bucket_name = argv[1];

  // Create aliases to make the code easier to read.
  namespace gcs = ::google::cloud::storage;

  // Create a client to communicate with Google Cloud Storage. This client
  // uses the default configuration for authentication and project id.
  auto client = gcs::Client();

  auto writer = client.WriteObject(bucket_name, "quickstart.txt");
  writer << "Hello World!";
  writer.Close();
  if (writer.metadata()) {
    std::cout << "Successfully created object: " << *writer.metadata() << "\n";
  } else {
    std::cerr << "Error creating object: " << writer.metadata().status()
              << "\n";
    return 1;
  }

  auto reader = client.ReadObject(bucket_name, "quickstart.txt");
  if (!reader) {
    std::cerr << "Error reading object: " << reader.status() << "\n";
    return 1;
  }

  std::string contents{std::istreambuf_iterator<char>{reader}, {}};
  std::cout << contents << "\n";

  return 0;
}
```
* [Google Cloud BigQuery](google/cloud/bigquery/README.md) [[quickstart]](google/cloud/bigquery/quickstart/README.md)
* [Google Cloud Bigtable](google/cloud/bigtable/README.md) [[quickstart]](google/cloud/bigtable/quickstart/README.md)
* [Google Cloud IAM](google/cloud/iam/README.md) [[quickstart]](google/cloud/iam/quickstart/README.md)
* [Google Cloud Pub/Sub](google/cloud/pubsub/README.md) [[quickstart]](google/cloud/pubsub/quickstart/README.md)
* [Secret Manager API](google/cloud/secretmanager/README.md) [[quickstart]](google/cloud/secretmanager/quickstart/README.md)
* [Google Cloud Spanner](google/cloud/spanner/README.md) [[quickstart]](google/cloud/spanner/quickstart/README.md)
* [Google Cloud Storage](google/cloud/storage/README.md) [[quickstart]](google/cloud/storage/quickstart/README.md)
* [Cloud Tasks API](google/cloud/tasks/README.md) [[quickstart]](google/cloud/tasks/quickstart/README.md)
