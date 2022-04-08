// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/endpoint.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/pubsublite/publisher_connection_impl.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;

TEST(EndpointFromZone, Basic) {
  EXPECT_THAT(EndpointFromZone(""), StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(EndpointFromZone("a-"), StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(EndpointFromZone("-a"), StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(EndpointFromZone("aaa"), StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(EndpointFromZone("us-central1-b").value(),
              "us-central1-pubsublite.googleapis.com");
}

TEST(EndpointFromRegion, Basic) {
  EXPECT_THAT(EndpointFromRegion(""), StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(EndpointFromRegion("aaa"),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(EndpointFromRegion("1a1"),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(EndpointFromRegion("us-central1").value(),
              "us-central1-pubsublite.googleapis.com");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
