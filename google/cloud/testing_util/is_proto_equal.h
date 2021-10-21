// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_IS_PROTO_EQUAL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_IS_PROTO_EQUAL_H

#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <google/protobuf/message.h>
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

absl::optional<std::string> CompareProtos(
    google::protobuf::Message const& arg,
    google::protobuf::Message const& value);

MATCHER_P(IsProtoEqual, value, "Checks whether protos are equal") {
  absl::optional<std::string> delta = CompareProtos(arg, value);
  if (delta.has_value()) {
    *result_listener << "\n" << *delta;
  }
  return !delta.has_value();
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_IS_PROTO_EQUAL_H
