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
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  // --- DEBUG START ---
  std::cout << "--- BINARY DEBUG START ---\n";
  const char* env_p = std::getenv("CURL_CA_BUNDLE");
  if (env_p) {
      std::cout << "CURL_CA_BUNDLE is set to: [" << env_p << "]\n";
      std::ifstream f(env_p);
      if (f.good()) {
          std::cout << "PASS: C++ std::ifstream can successfully open this path.\n";
          // Optional: Print first line to prove we are reading the right file
          std::string line;
          std::getline(f, line);
          std::cout << "First line of file: " << line << "\n";
      } else {
          std::cout << "FAIL: C++ std::ifstream FAILED to open this path.\n";
      }
  } else {
      std::cout << "FAIL: CURL_CA_BUNDLE is NOT set in the environment.\n";
  }
  std::cout << "--- BINARY DEBUG END ---\n";
  // --- DEBUG END ---

  if (argc != 2) {
    std::cerr << "Missing bucket name.\n";
    std::cerr << "Usage: quickstart <bucket-name>\n";
    return 1;
  }
  std::string const bucket_name = argv[1];

  // Create a client to communicate with Google Cloud Storage. This client
  // uses the default configuration for authentication and project id.
  auto client = google::cloud::storage::Client();

  auto writer = client.WriteObject(bucket_name, "quickstart.txt");
  writer << "Hello World!";
  writer.Close();
  if (!writer.metadata()) {
    std::cerr << "Error creating object: " << writer.metadata().status()
              << "\n";
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
