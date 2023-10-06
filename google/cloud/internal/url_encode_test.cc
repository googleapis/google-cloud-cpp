// Copyright 2023 Google LLC
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

#include "google/cloud/internal/url_encode.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(UrlEncode, Simple) {
  auto unencoded_string = "projects/*/resource/*";

  auto result = UrlEncode(unencoded_string);

  auto encoded_string = "projects%2F*%2Fresource%2F*";
  ASSERT_THAT(result, encoded_string);
}

TEST(UrlEncode, MultipleReplacements) {
  auto unencoded_string = "%>/@";

  auto result = UrlEncode(unencoded_string);

  auto encoded_string = "%25%3E%2F%40";
  ASSERT_THAT(result, encoded_string);
}

TEST(UrlDecode, Simple) {
  auto encoded_string = "projects%2F*%2Fresource%2F*";

  auto result = UrlDecode(encoded_string);

  auto unencoded_string = "projects/*/resource/*";
  ASSERT_THAT(result, unencoded_string);
}

TEST(UrlDecode, MultipleReplacements) {
  auto encoded_string = "%25%3E%2F%40";

  auto result = UrlDecode(encoded_string);

  auto unencoded_string = "%>/@";
  ASSERT_THAT(result, unencoded_string);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace internal
}  // namespace cloud
}  // namespace google
