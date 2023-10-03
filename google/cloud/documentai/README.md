# Cloud Document AI API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Document AI API][cloud-service], a service that uses machine learning on
a scalable cloud-based platform to help your organization efficiently scan,
analyze, and understand documents.

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
#include "google/cloud/documentai/v1/document_processor_client.h"
#include "google/cloud/location.h"
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
  if (argc != 5) {
    std::cerr << "Usage: " << argv[0]
              << " project-id location-id processor-id filename (PDF only)\n";
    return 1;
  }

  std::string const location_id = argv[2];
  if (location_id != "us" && location_id != "eu") {
    std::cerr << "location-id must be either 'us' or 'eu'\n";
    return 1;
  }
  auto const location = google::cloud::Location(argv[1], location_id);

  namespace documentai = ::google::cloud::documentai_v1;
  auto client = documentai::DocumentProcessorServiceClient(
      documentai::MakeDocumentProcessorServiceConnection(
          location.location_id()));

  google::cloud::documentai::v1::ProcessRequest req;
  req.set_name(location.FullName() + "/processors/" + argv[3]);
  req.set_skip_human_review(true);
  auto& doc = *req.mutable_raw_document();
  doc.set_mime_type("application/pdf");
  std::ifstream is(argv[4]);
  doc.set_content(std::string{std::istreambuf_iterator<char>(is), {}});

  auto resp = client.ProcessDocument(std::move(req));
  if (!resp) throw std::move(resp).status();
  std::cout << resp->document().text() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Document AI API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service]: https://cloud.google.com/document-ai
[cloud-service-docs]: https://cloud.google.com/document-ai/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/documentai/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/documentai
