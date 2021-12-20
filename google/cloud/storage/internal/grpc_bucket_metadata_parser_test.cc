// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/grpc_bucket_metadata_parser.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;

TEST(GrpcClientBucketMetadata, BucketBillingRoundtrip) {
  auto constexpr kText = R"pb(
    requester_pays: true
  )pb";
  google::storage::v2::Bucket::Billing start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = BucketBilling{true};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
