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

#include "google/cloud/storage/async/idempotency_policy.h"
#include "google/protobuf/text_format.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::protobuf::TextFormat;
using ::testing::NotNull;

template <typename M>
auto TestMessage(char const* text, M m) {
  EXPECT_TRUE(TextFormat::ParseFromString(text, &m));
  return m;
}

TEST(IdempotencyPolicy, Strict) {
  auto policy = MakeStrictIdempotencyPolicy();
  ASSERT_THAT(policy, NotNull());

  EXPECT_EQ(policy->ReadObject(google::storage::v2::ReadObjectRequest{}),
            Idempotency::kIdempotent);

  EXPECT_EQ(policy->InsertObject(google::storage::v2::WriteObjectRequest{}),
            Idempotency::kNonIdempotent);

  EXPECT_EQ(policy->InsertObject(TestMessage(
                R"pb(write_object_spec { if_generation_match: 42 })pb",
                google::storage::v2::WriteObjectRequest{})),
            Idempotency::kIdempotent);
  EXPECT_EQ(policy->InsertObject(TestMessage(
                R"pb(write_object_spec { if_metageneration_match: 42 })pb",
                google::storage::v2::WriteObjectRequest{})),
            Idempotency::kIdempotent);
  EXPECT_EQ(policy->InsertObject(google::storage::v2::WriteObjectRequest{}),
            Idempotency::kNonIdempotent);

  EXPECT_EQ(policy->WriteObject(google::storage::v2::WriteObjectRequest{}),
            Idempotency::kIdempotent);

  EXPECT_EQ(policy->ComposeObject(google::storage::v2::ComposeObjectRequest{}),
            Idempotency::kNonIdempotent);
  EXPECT_EQ(policy->ComposeObject(
                TestMessage(R"pb(if_generation_match: 42)pb",
                            google::storage::v2::ComposeObjectRequest{})),
            Idempotency::kIdempotent);
  EXPECT_EQ(policy->ComposeObject(
                TestMessage(R"pb(if_metageneration_match: 42)pb",
                            google::storage::v2::ComposeObjectRequest{})),
            Idempotency::kIdempotent);

  EXPECT_EQ(policy->DeleteObject(google::storage::v2::DeleteObjectRequest{}),
            Idempotency::kNonIdempotent);
  EXPECT_EQ(
      policy->DeleteObject(TestMessage(
          R"pb(generation: 42)pb", google::storage::v2::DeleteObjectRequest{})),
      Idempotency::kIdempotent);
  EXPECT_EQ(policy->DeleteObject(
                TestMessage(R"pb(if_generation_match: 42)pb",
                            google::storage::v2::DeleteObjectRequest{})),
            Idempotency::kIdempotent);
  EXPECT_EQ(policy->DeleteObject(
                TestMessage(R"pb(if_metageneration_match: 42)pb",
                            google::storage::v2::DeleteObjectRequest{})),
            Idempotency::kIdempotent);

  EXPECT_EQ(policy->RewriteObject(google::storage::v2::RewriteObjectRequest{}),
            Idempotency::kIdempotent);
}

TEST(IdempotencyPolicy, AlwaysRetry) {
  auto policy = MakeAlwaysRetryIdempotencyPolicy();
  ASSERT_THAT(policy, NotNull());
  EXPECT_EQ(policy->ReadObject(google::storage::v2::ReadObjectRequest{}),
            Idempotency::kIdempotent);
  EXPECT_EQ(policy->InsertObject(google::storage::v2::WriteObjectRequest{}),
            Idempotency::kIdempotent);
  EXPECT_EQ(policy->WriteObject(google::storage::v2::WriteObjectRequest{}),
            Idempotency::kIdempotent);
  EXPECT_EQ(policy->ComposeObject(google::storage::v2::ComposeObjectRequest{}),
            Idempotency::kIdempotent);
  EXPECT_EQ(policy->DeleteObject(google::storage::v2::DeleteObjectRequest{}),
            Idempotency::kIdempotent);
  EXPECT_EQ(policy->RewriteObject(google::storage::v2::RewriteObjectRequest{}),
            Idempotency::kIdempotent);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
