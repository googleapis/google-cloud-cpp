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

#include "generator/integration_tests/golden/v1/request_id_connection_idempotency_policy.h"
#include <gmock/gmock.h>
#include <utility>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::test::requestid::v1::CreateFooRequest;
using ::google::test::requestid::v1::ListFoosRequest;
using ::google::test::requestid::v1::RenameFooRequest;

TEST(RequestIdIdempotency, CreateFoo) {
  auto policy = MakeDefaultRequestIdServiceConnectionIdempotencyPolicy();
  CreateFooRequest request;
  EXPECT_EQ(policy->CreateFoo(request), Idempotency::kNonIdempotent);
  request.set_request_id("test-request-id");
  EXPECT_EQ(policy->CreateFoo(request), Idempotency::kIdempotent);
}

TEST(RequestIdIdempotency, ListFoos) {
  auto policy = MakeDefaultRequestIdServiceConnectionIdempotencyPolicy();
  ListFoosRequest request;
  EXPECT_EQ(policy->ListFoos(request), Idempotency::kNonIdempotent);
  request.set_request_id("test-request-id");
  EXPECT_EQ(policy->ListFoos(request), Idempotency::kNonIdempotent);
}

TEST(RequestIdIdempotency, RenameFoo) {
  auto policy = MakeDefaultRequestIdServiceConnectionIdempotencyPolicy();
  RenameFooRequest request;
  EXPECT_EQ(policy->RenameFoo(request), Idempotency::kNonIdempotent);
  request.set_request_id("test-request-id");
  EXPECT_EQ(policy->RenameFoo(request), Idempotency::kIdempotent);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google
