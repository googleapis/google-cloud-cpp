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
  const auto *unencoded_string = "projects/*/resource/*";

  auto result = internal::UrlEncode(unencoded_string);

  const auto *encoded_string = "projects%2F*%2Fresource%2F*";
  ASSERT_THAT(result, encoded_string);
}

TEST(UrlEncode, MultipleReplacements) {
  const auto *unencoded_string = "%>/@";

  auto result = internal::UrlEncode(unencoded_string);

  const auto *encoded_string = "%25%3E%2F%40";
  ASSERT_THAT(result, encoded_string);
}

TEST(UrlDecode, Simple) {
  const auto *encoded_string = "projects%2F*%2Fresource%2F*";

  auto result = internal::UrlDecode(encoded_string);

  const auto *unencoded_string = "projects/*/resource/*";
  ASSERT_THAT(result, unencoded_string);
}

TEST(UrlDecode, MultipleReplacements) {
  const auto *encoded_string = "%25%3E%2F%40";

  auto result = internal::UrlDecode(encoded_string);

  const auto *unencoded_string = "%>/@"; 
 ASSERT_THAT(result, unencoded_string);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace internal
}  // namespace cloud
}  // namespace google
