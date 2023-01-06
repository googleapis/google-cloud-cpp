// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

struct Placeholder : public GenericRequest<Placeholder> {};

TEST(GenericRequestTest, SetOptionRValueFirstBase) {
  Placeholder req;
  req.set_option(QuotaUser("user1"));
  EXPECT_TRUE(req.HasOption<QuotaUser>());
  EXPECT_EQ("user1", req.GetOption<QuotaUser>().value());
}

TEST(GenericRequestTest, SetOptionLValueFirstBase) {
  Placeholder req;
  QuotaUser arg("user1");
  req.set_option(arg);
  EXPECT_TRUE(req.HasOption<QuotaUser>());
  EXPECT_EQ("user1", req.GetOption<QuotaUser>().value());
}

TEST(GenericRequestTest, SetOptionRValueLastBase) {
  Placeholder req;
  req.set_option(CustomHeader("header1", "val1"));
  EXPECT_TRUE(req.HasOption<CustomHeader>());
  EXPECT_EQ("header1", req.GetOption<CustomHeader>().custom_header_name());
}

TEST(GenericRequestTest, SetOptionLValueLastBase) {
  Placeholder req;
  CustomHeader arg("header1", "val1");
  req.set_option(arg);
  EXPECT_TRUE(req.HasOption<CustomHeader>());
  EXPECT_EQ("header1", req.GetOption<CustomHeader>().custom_header_name());
}

TEST(GenericRequestTest, IgnoreOptionsBundle) {
  Placeholder req;
  req.set_multiple_options(Options{}, CustomHeader{"header1", "val1"});
  req.set_multiple_options(CustomHeader{"header1", "val1"}, Options{});
  req.set_multiple_options(CustomHeader{"header1", "val1"}, Options{},
                           CustomHeader{"header1", "val2"});
  Options bundle;
  auto const& cref = bundle;
  req.set_multiple_options(cref, CustomHeader{"header1", "val1"});
  req.set_multiple_options(CustomHeader{"header1", "val1"}, cref);
  req.set_multiple_options(CustomHeader{"header1", "val1"}, cref,
                           CustomHeader{"header1", "val2"});
  auto& ref = bundle;
  req.set_multiple_options(ref, CustomHeader{"header1", "val1"});
  req.set_multiple_options(CustomHeader{"header1", "val1"}, ref);
  req.set_multiple_options(CustomHeader{"header1", "val1"}, ref,
                           CustomHeader{"header1", "val2"});
  ASSERT_TRUE(req.HasOption<CustomHeader>());
  EXPECT_EQ("header1", req.GetOption<CustomHeader>().custom_header_name());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
