// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H_

#include "google/cloud/internal/random.h"
#include "google/cloud/storage/well_known_headers.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class StorageIntegrationTest : public ::testing::Test {
 protected:
  std::string MakeRandomObjectName();

  std::string LoremIpsum() const;

  EncryptionKeyData MakeEncryptionKeyData();

  std::string MakeRandomBucketName();

  void WriteRandomLines(std::ostream& upload, std::ostream& local);

  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();
};
}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H_
