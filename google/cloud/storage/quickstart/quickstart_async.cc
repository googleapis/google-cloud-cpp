// Copyright 2024 Google LLC
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

#include "google/cloud/storage/async/client.h"
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Missing bucket name.\n";
    std::cerr << "Usage: quickstart <bucket-name>\n";
    return 1;
  }
  std::string const bucket_name = argv[1];

  namespace gcs_ex = ::google::cloud::storage_experimental;

  // Create a client to communicate with Google Cloud Storage. This client
  // uses the default configuration for authentication and project id.
  auto client = gcs_ex::AsyncClient();

  auto constexpr kObjectName = "quickstart-async.txt";
  auto done = client
                  .InsertObject(gcs_ex::BucketName(bucket_name), kObjectName,
                                "Hello World!")
                  .then([](auto f) {
                    auto metadata = f.get();
                    if (!metadata) {
                      std::cerr
                          << "Error creating object: " << metadata.status()
                          << "\n";
                      std::exit(1);
                    }
                    std::cout << "Successfully created object "
                              << metadata->DebugString() << "\n";
                  });
  done.get();

  done = client
             .ReadObjectRange(gcs_ex::BucketName(bucket_name), kObjectName, 0,
                              1000)
             .then([](auto f) {
               auto payload = f.get();
               if (!payload) {
                 std::cerr << "Error reading object: " << payload.status()
                           << "\n";
                 std::exit(1);
               }
               if (payload->metadata()) {
                 std::cout << "The object metadata is "
                           << payload->metadata()->DebugString() << "\n";
               }
               std::cout << "Object contents:\n";
               for (auto const& s : payload->contents()) {
                 std::cout << s;
               }
               std::cout << "\n";
             });
  done.get();

  return 0;
}
