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
#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_REST_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_REST_CLIENT_H

#include "google/cloud/internal/rest_client.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

class MockRestClient : public rest_internal::RestClient {
 public:
  MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Delete,
              (rest_internal::RestContext&, rest_internal::RestRequest const&),
              (override));
  MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Get,
              (rest_internal::RestContext&, rest_internal::RestRequest const&),
              (override));
  MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Patch,
              (rest_internal::RestContext&, rest_internal::RestRequest const&,
               std::vector<absl::Span<char const>> const&),
              (override));
  MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Post,
              (rest_internal::RestContext&, rest_internal::RestRequest const&,
               std::vector<absl::Span<char const>> const&),
              (override));
  MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Post,
              (rest_internal::RestContext&,
               rest_internal::RestRequest const& request,
               (std::vector<std::pair<std::string, std::string>> const&)),
              (override));
  MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Put,
              (rest_internal::RestContext&, rest_internal::RestRequest const&,
               std::vector<absl::Span<char const>> const&),
              (override));
};

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_REST_CLIENT_H
