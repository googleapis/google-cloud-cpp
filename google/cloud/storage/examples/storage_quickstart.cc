// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! [full quickstart]
//! [header]
#include "google/cloud/storage/client.h"
//! [header]
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Missing project id and/or bucket name." << std::endl;
    std::cerr << "Usage: storage_quickstart <bucket-name> <project-id>"
              << std::endl;
    return 1;
  }
  std::string bucket_name = argv[1];
  std::string project_id = argv[2];

  //! [namespace alias]
  namespace gcs = google::cloud::storage;
  //! [namespace alias]

  // Create a client to communicate with Google Cloud Storage.
  //! [create client]
  gcs::Client client;
  //! [create client]

  //! [create bucket]
  gcs::BucketMetadata metadata = client.CreateBucketForProject(
      bucket_name, project_id,
      gcs::BucketMetadata()
          .set_location("us-east1")
          .set_storage_class(gcs::storage_class::Regional()));

  std::cout << "Created bucket " << metadata.name() << std::endl;
  //! [create bucket]

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
//! [full quickstart]
