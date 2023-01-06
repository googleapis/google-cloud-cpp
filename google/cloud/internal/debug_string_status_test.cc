// Copyright 2022 Google LLC
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

#include "google/cloud/internal/debug_string_status.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/tracing_options.h"
#include <google/rpc/error_details.pb.h>
#include <google/rpc/status.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::HasSubstr;
using ::testing::StartsWith;

TEST(DebugStringStatus, Basic) {
  auto detail = google::rpc::BadRequest();
  auto& field_violation = *detail.add_field_violations();
  field_violation.set_field("my_field");
  field_violation.set_description("it is immutable");
  auto proto = google::rpc::Status();
  proto.set_code(static_cast<std::int32_t>(StatusCode::kInvalidArgument));
  proto.set_message("oh noes!");
  proto.add_details()->PackFrom(detail);

  auto const status = MakeStatusFromRpcError(proto);
  auto const actual = DebugString(status, TracingOptions());
  EXPECT_THAT(actual, StartsWith("INVALID_ARGUMENT: oh noes! + "));
  EXPECT_THAT(actual, HasSubstr("my_field"));
  EXPECT_THAT(actual, HasSubstr("it is immutable"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
