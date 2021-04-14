// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_CLIENT_UNIT_TEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_CLIENT_UNIT_TEST_H

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

/// Common setup for google::cloud::storage::Client unit tests
class ClientUnitTest : public ::testing::Test {
 public:
  ClientUnitTest();

 protected:
  google::cloud::storage::Client ClientForMock();

  std::shared_ptr<testing::MockClient> mock_;
  ClientOptions client_options_;
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_CLIENT_UNIT_TEST_H
