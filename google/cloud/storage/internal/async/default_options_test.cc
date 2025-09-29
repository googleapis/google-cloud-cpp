// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/async/idempotency_policy.h"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/async/resume_policy.h"
#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/common_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;

TEST(DefaultOptionsAsync, BufferedWaterMarks) {
  auto const options = DefaultOptionsAsync({});
  auto const lwm = options.get<storage::BufferedUploadLwmOption>();
  auto const hwm = options.get<storage::BufferedUploadHwmOption>();
  EXPECT_GE(lwm, 256 * 1024);
  EXPECT_LT(lwm, hwm);
}

TEST(DefaultOptionsAsync, Endpoint) {
  auto const options = DefaultOptionsAsync({});
  // We use EndpointOption as a canary to test that most options are set.
  EXPECT_TRUE(options.has<EndpointOption>());
  EXPECT_THAT(options.get<EndpointOption>(), Not(IsEmpty()));
}

TEST(DefaultOptionsAsync, ResumePolicy) {
  auto const options = DefaultOptionsAsync({});
  // Verify the ResumePolicyOption is set and it creates valid policies.
  EXPECT_TRUE(options.has<storage::ResumePolicyOption>());
  auto factory = options.get<storage::ResumePolicyOption>();
  EXPECT_TRUE(static_cast<bool>(factory));
  auto policy = factory();
  EXPECT_THAT(policy, NotNull());
}

TEST(DefaultOptionsAsync, IdempotencyPolicy) {
  auto const options = DefaultOptionsAsync({});
  // Verify the AsyncIdempotencyPolicyOption is set and it creates valid
  // policies.
  EXPECT_TRUE(options.has<storage::AsyncIdempotencyPolicyOption>());
  auto factory = options.get<storage::AsyncIdempotencyPolicyOption>();
  EXPECT_TRUE(static_cast<bool>(factory));
  auto policy = factory();
  EXPECT_THAT(policy, NotNull());
}

TEST(DefaultOptionsAsync, Hashes) {
  auto const options = DefaultOptionsAsync({});
  EXPECT_TRUE(options.get<storage::EnableCrc32cValidationOption>());
  EXPECT_FALSE(options.get<storage::EnableMD5ValidationOption>());
  EXPECT_FALSE(options.has<storage::UseCrc32cValueOption>());
  EXPECT_FALSE(options.has<storage::UseMD5ValueOption>());
}

TEST(DefaultOptionsAsync, Adjust) {
  auto const options =
      DefaultOptionsAsync(Options{}
                              .set<storage::BufferedUploadLwmOption>(16 * 1024)
                              .set<storage::BufferedUploadHwmOption>(8 * 1024));
  auto const lwm = options.get<storage::BufferedUploadLwmOption>();
  auto const hwm = options.get<storage::BufferedUploadHwmOption>();
  EXPECT_GE(lwm, 256 * 1024);
  EXPECT_LT(lwm, hwm);
}

TEST(DefaultOptionsAsync, MaximumRangeSizeOption) {
  auto const options = DefaultOptionsAsync({});
  auto const max_range_size_option =
      options.get<storage::MaximumRangeSizeOption>();
  EXPECT_EQ(max_range_size_option, 128 * 1024 * 1024L);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
