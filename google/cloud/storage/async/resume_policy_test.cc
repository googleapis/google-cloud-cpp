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

#include "google/cloud/storage/async/resume_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::NotNull;

TEST(ResumePolicy, LimitedErrorCountResumePolicy) {
  auto factory = LimitedErrorCountResumePolicy(0);
  auto p0 = factory();
  ASSERT_THAT(p0, NotNull());
  EXPECT_EQ(p0->OnFinish(TransientError()), ResumePolicy::kStop);
  EXPECT_EQ(p0->OnFinish(TransientError()), ResumePolicy::kStop);
  p0 = factory();
  ASSERT_THAT(p0, NotNull());
  EXPECT_EQ(p0->OnFinish(TransientError()), ResumePolicy::kStop);

  factory = LimitedErrorCountResumePolicy(3);
  auto p3 = factory();
  ASSERT_THAT(p3, NotNull());
  EXPECT_EQ(p3->OnFinish(TransientError()), ResumePolicy::kContinue);
  EXPECT_EQ(p3->OnFinish(TransientError()), ResumePolicy::kContinue);
  EXPECT_EQ(p3->OnFinish(TransientError()), ResumePolicy::kContinue);
  EXPECT_EQ(p3->OnFinish(TransientError()), ResumePolicy::kStop);
  EXPECT_EQ(p3->OnFinish(TransientError()), ResumePolicy::kStop);

  p3 = factory();
  ASSERT_THAT(p3, NotNull());
  EXPECT_EQ(p3->OnFinish(TransientError()), ResumePolicy::kContinue);
  EXPECT_EQ(p3->OnFinish(TransientError()), ResumePolicy::kContinue);
  EXPECT_EQ(p3->OnFinish(TransientError()), ResumePolicy::kContinue);
  EXPECT_EQ(p3->OnFinish(TransientError()), ResumePolicy::kStop);
  EXPECT_EQ(p3->OnFinish(TransientError()), ResumePolicy::kStop);
}

TEST(ResumePolicy, StopOnConsecutiveErrorsResumePolicy) {
  auto policy = StopOnConsecutiveErrorsResumePolicy()();
  ASSERT_THAT(policy, NotNull());
  policy->OnStartSuccess();
  EXPECT_EQ(policy->OnFinish(TransientError()), ResumePolicy::kContinue);
  policy->OnStartSuccess();
  EXPECT_EQ(policy->OnFinish(TransientError()), ResumePolicy::kContinue);
  policy->OnStartSuccess();
  EXPECT_EQ(policy->OnFinish(TransientError()), ResumePolicy::kContinue);
  policy->OnStartSuccess();
  EXPECT_EQ(policy->OnFinish(TransientError()), ResumePolicy::kContinue);
  EXPECT_EQ(policy->OnFinish(TransientError()), ResumePolicy::kStop);
  EXPECT_EQ(policy->OnFinish(TransientError()), ResumePolicy::kStop);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
