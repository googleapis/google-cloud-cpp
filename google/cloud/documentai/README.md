# Cloud Document AI API C++ Client Library

:construction:

:warning: This library will not be declared GA until the quickstart is operable.

This directory contains an idiomatic C++ client library for the
[Cloud Document AI API][cloud-service], a service that uses machine
learning on a scalable cloud-based platform to help your organization
efficiently scan, analyze, and understand documents.

This library is **experimental**. Its APIs are subject to change without notice.

## Supported Platforms

* Windows, macOS, Linux
* C++11 (and higher) compilers (we test with GCC >= 5.4, Clang >= 6.0, and
  MSVC >= 2017)
* Environments with or without exceptions
* Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

* Official documentation about the [Cloud Document AI API][cloud-service-docs] service
* [Reference doxygen documentation][doxygen-link] for each release of this
  client library
* Detailed header comments in our [public `.h`][source-link] files

[cloud-service]: https://cloud.google.com/document-ai
[cloud-service-docs]: https://cloud.google.com/document-ai/docs
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-documentai/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/documentai

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->
```cc
#include "google/cloud/documentai/document_processor_client.h"
#include "google/cloud/common_options.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " project-id location-id processor-id\n";
    return 1;
  }
  std::string const location = argv[2];
  if (location != "us" && location != "eu") {
    std::cerr << "location-id must be either 'us' or 'eu'\n";
    return 1;
  }

  namespace gc = ::google::cloud;
  auto options = gc::Options{}.set<gc::EndpointOption>(
      location + "-documentai.googleapis.com");
  options.set<google::cloud::TracingComponentsOption>({"rpc"});

  namespace documentai = ::google::cloud::documentai;
  auto client = documentai::DocumentProcessorServiceClient(
      documentai::MakeDocumentProcessorServiceConnection(options));

  auto const resource = std::string{"projects/"} + argv[1] + "/locations/" +
                        argv[2] + "/processors/" + argv[3];

  google::cloud::documentai::v1::ProcessRequest req;
  req.set_name(resource);
  req.set_skip_human_review(true);
  auto& doc = *req.mutable_inline_document();
  doc.set_mime_type("application/pdf");
  doc.set_uri("gs://cloud-samples-data/documentai/invoice.pdf");

  auto resp = client.ProcessDocument(std::move(req));
  if (!resp) throw std::runtime_error(resp.status().message());
  std::cout << resp->document().text() << "\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
```
<!-- inject-quickstart-end -->

* Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
* Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
* Developers wanting to compile the library just to run some of the examples or
  tests should read the current document.
* Contributors and developers to `google-cloud-cpp` should consult the guide to
  [setup a development workstation][howto-setup-dev-workstation].

[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.
