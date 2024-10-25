// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/protobuf/util/message_differencer.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

absl::optional<std::string> CompareProtos(
    google::protobuf::Message const& arg,
    google::protobuf::Message const& value) {
  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  auto const result = differencer.Compare(arg, value);
  if (result) return absl::nullopt;
  return delta;
}

absl::optional<std::string> CompareProtosApproximately(
    google::protobuf::Message const& arg,
    google::protobuf::Message const& value) {
  std::string delta;
  google::protobuf::util::DefaultFieldComparator comparator;
  comparator.set_float_comparison(
      google::protobuf::util::DefaultFieldComparator::APPROXIMATE);
  // Keep the comparator's default fraction and margin.
  google::protobuf::util::MessageDifferencer differencer;
  differencer.set_field_comparator(&comparator);
  differencer.ReportDifferencesToString(&delta);
  auto const result = differencer.Compare(arg, value);
  if (result) return absl::nullopt;
  return delta;
}

absl::optional<std::string> CompareProtosApproximately(
    google::protobuf::Message const& arg,
    google::protobuf::Message const& value, double fraction, double margin) {
  std::string delta;
  google::protobuf::util::DefaultFieldComparator comparator;
  comparator.set_float_comparison(
      google::protobuf::util::DefaultFieldComparator::APPROXIMATE);
  comparator.SetDefaultFractionAndMargin(fraction, margin);
  google::protobuf::util::MessageDifferencer differencer;
  differencer.set_field_comparator(&comparator);
  differencer.ReportDifferencesToString(&delta);
  auto const result = differencer.Compare(arg, value);
  if (result) return absl::nullopt;
  return delta;
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
