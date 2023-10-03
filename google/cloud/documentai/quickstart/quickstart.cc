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

//! [all]
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
//! [all]
