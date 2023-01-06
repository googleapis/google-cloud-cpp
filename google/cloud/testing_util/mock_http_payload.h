// Copyright 2022 Google LLC
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
//
#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_HTTP_PAYLOAD_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_HTTP_PAYLOAD_H

#include "google/cloud/internal/http_payload.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <iterator>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

class MockHttpPayload : public rest_internal::HttpPayload {
 public:
  ~MockHttpPayload() override = default;
  MOCK_METHOD(bool, HasUnreadData, (), (const, override));
  MOCK_METHOD(StatusOr<std::size_t>, Read, (absl::Span<char> buffer),
              (override));
};

template <typename Collection>
std::unique_ptr<rest_internal::HttpPayload> MakeMockHttpPayloadSuccess(
    Collection contents) {
  auto mock = absl::make_unique<MockHttpPayload>();
  // This is shared by the next two mocking functions.
  auto c = std::make_shared<Collection>(std::forward<Collection>(contents));
  EXPECT_CALL(*mock, HasUnreadData).WillRepeatedly([c] { return !c->empty(); });
  EXPECT_CALL(*mock, Read).WillRepeatedly([c](absl::Span<char> buffer) {
    // Copy as much as possible from `c` into `buffer`.
    auto const n = (std::min)(buffer.size(), c->size());
    auto const end = std::next(c->begin(), n);
    std::copy(c->begin(), end, buffer.begin());
    // Remove the copied bytes from `c`:
    auto tmp = Collection(end, c->end());
    *c = std::forward<Collection>(tmp);
    return n;
  });
  return mock;
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_HTTP_PAYLOAD_H
