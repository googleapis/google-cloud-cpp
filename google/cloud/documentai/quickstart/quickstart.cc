// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
