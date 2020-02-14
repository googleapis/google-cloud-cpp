// Copyright 2020 Google LLC
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

#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/wrappers.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
namespace {

using ::testing::Not;

TEST(IsProtoEqual, Basic) {
  ::google::protobuf::StringValue actual;
  actual.set_value("Hello World");
  ::google::protobuf::StringValue not_actual;

  EXPECT_THAT(actual, IsProtoEqual(actual));
  EXPECT_THAT(actual, Not(IsProtoEqual(not_actual)));
}

}  // namespace
}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
