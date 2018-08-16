// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/http_response.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;
TEST(HttpResponseTest, OStream) {
  HttpResponse response{
      404, "some-payload", {{"header1", "value1"}, {"header2", "value2"}}};

  std::ostringstream os;
  os << response;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("404"));
  EXPECT_THAT(actual, HasSubstr("some-payload"));
  EXPECT_THAT(actual, HasSubstr("header1: value1"));
  EXPECT_THAT(actual, HasSubstr("header2: value2"));
}
}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
