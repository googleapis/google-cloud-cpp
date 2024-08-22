// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_HASH_VALIDATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_HASH_VALIDATOR_H

#include "google/cloud/storage/internal/hash_validator.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class MockHashValidator : public storage::internal::HashValidator {
 public:
  MOCK_METHOD(std::string, Name, (), (const, override));
  MOCK_METHOD(void, ProcessMetadata, (ObjectMetadata const&), (override));
  MOCK_METHOD(void, ProcessHashValues, (storage::internal::HashValues const&),
              (override));
  MOCK_METHOD(HashValidator::Result, Finish, (storage::internal::HashValues),
              (ref(&&), override));
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_HASH_VALIDATOR_H
