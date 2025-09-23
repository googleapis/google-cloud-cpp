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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_RESUME_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_RESUME_POLICY_H

#include "google/cloud/storage/async/resume_policy.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class MockResumePolicy : public storage_experimental::ResumePolicy {
 public:
  ~MockResumePolicy() override = default;

  MOCK_METHOD(std::unique_ptr<storage_experimental::ResumePolicy>, clone, (),
              (const, override));
  MOCK_METHOD(void, OnStartSuccess, (), (override));
  MOCK_METHOD(ResumePolicy::Action, OnFinish, (Status const&), (override));
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_RESUME_POLICY_H
