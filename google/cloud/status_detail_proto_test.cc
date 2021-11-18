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

#include "google/cloud/status_detail_proto.h"
#include "google/cloud/grpc_error_delegate.h"
#include <google/rpc/error_details.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(GetStatusDetailProto, NoDetails) {
  auto details = GetStatusDetailProto<google::rpc::ErrorInfo>(Status{});
  EXPECT_FALSE(details.has_value());
  details = GetStatusDetailProto<google::rpc::ErrorInfo>(
      Status{StatusCode::kUnknown, "foo"});
  EXPECT_FALSE(details.has_value());
}

TEST(GetStatusDetailProto, DetailsExist) {
  google::rpc::ErrorInfo error_info;
  error_info.set_reason("the reason");
  error_info.set_domain("the domain");

  google::rpc::Status proto;
  proto.set_code(static_cast<int>(StatusCode::kUnknown));
  proto.set_message("oops");
  proto.add_details()->PackFrom(error_info);

  auto status = MakeStatusFromRpcError(proto);
  auto actual = GetStatusDetailProto<google::rpc::ErrorInfo>(status);
  EXPECT_TRUE(actual.has_value());
  EXPECT_EQ(actual->reason(), "the reason");
  EXPECT_EQ(actual->domain(), "the domain");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
