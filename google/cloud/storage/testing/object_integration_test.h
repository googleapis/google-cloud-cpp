// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_OBJECT_INTEGRATION_TEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_OBJECT_INTEGRATION_TEST_H

#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/status_or.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class ObjectIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  static StatusOr<std::size_t> GetNumOpenFiles();

  void SetUp() override;
  std::string MakeEntityName() const;

  std::string project_id_;
  std::string bucket_name_;
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_OBJECT_INTEGRATION_TEST_H
