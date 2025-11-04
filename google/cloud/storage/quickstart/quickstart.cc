// Copyright 2018 Google LLC
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
#include "google/cloud/storage/client.h"
#include "google/cloud/common_options.h" // Required for CARootsFilePathOption
#include <curl/curl.h>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  // --- DEBUG START ---
  std::cout << "--- BINARY DEBUG START ---\n";
  auto* vinfo = curl_version_info(CURLVERSION_NOW);
  std::cout << "Libcurl Version: " << vinfo->version << "\n";
  std::cout << "SSL Backend: " << vinfo->ssl_version << "\n";
  
  const char* env_p = std::getenv("CURL_CA_BUNDLE");
  std::string ca_path;
  if (env_p) {
      ca_path = std::string(env_p);
      std::cout << "CURL_CA_BUNDLE found: [" << ca_path << "]\n";
  } else {
      std::cout << "FAIL: CURL_CA_BUNDLE is NOT set.\n";
  }
  std::cout << "--- BINARY DEBUG END ---\n";
  // --- DEBUG END ---

  if (argc != 2) {
    std::cerr << "Missing bucket name.\n";
    std::cerr << "Usage: quickstart <bucket-name>\n";
    return 1;
  }
  std::string const bucket_name = argv[1];

  // Configure options explicitly
  auto options = google::cloud::Options{};
  if (!ca_path.empty()) {
      std::cout << "Forcing CARootsFilePathOption to: " << ca_path << "\n";
      options.set<google::cloud::CARootsFilePathOption>(ca_path);
  }

  // Create client with explicit options
  auto client = google::cloud::storage::Client(options);

  auto writer = client.WriteObject(bucket_name, "quickstart.txt");
  writer << "Hello World!";
  writer.Close();
  if (!writer.metadata()) {
    std::cerr << "Error creating object: " << writer.metadata().status() << "\n";
    return 1;
  }
  std::cout << "Successfully created object: " << *writer.metadata() << "\n";

  auto reader = client.ReadObject(bucket_name, "quickstart.txt");
  if (!reader) {
    std::cerr << "Error reading object: " << reader.status() << "\n";
    return 1;
  }

  std::string contents{std::istreambuf_iterator<char>{reader}, {}};
  std::cout << contents << "\n";

  return 0;
}
//! [all]
