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

#include "google/cloud/storage/internal/generic_request.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

struct Dummy : public GenericRequest<Dummy> {};

TEST(GenericRequestTest, SetOptionRValueFirstBase) {
  Dummy req;
  req.set_option(QuotaUser("user1"));
  EXPECT_TRUE(req.HasOption<QuotaUser>());
  EXPECT_EQ("user1", req.GetOption<QuotaUser>().value());
}

TEST(GenericRequestTest, SetOptionLValueFirstBase) {
  Dummy req;
  QuotaUser arg("user1");
  req.set_option(arg);
  EXPECT_TRUE(req.HasOption<QuotaUser>());
  EXPECT_EQ("user1", req.GetOption<QuotaUser>().value());
}

TEST(GenericRequestTest, SetOptionRValueLastBase) {
  Dummy req;
  req.set_option(CustomHeader("header1", "val1"));
  EXPECT_TRUE(req.HasOption<CustomHeader>());
  EXPECT_EQ("header1", req.GetOption<CustomHeader>().custom_header_name());
}

TEST(GenericRequestTest, SetOptionLValueLastBase) {
  Dummy req;
  CustomHeader arg("header1", "val1");
  req.set_option(arg);
  EXPECT_TRUE(req.HasOption<CustomHeader>());
  EXPECT_EQ("header1", req.GetOption<CustomHeader>().custom_header_name());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
