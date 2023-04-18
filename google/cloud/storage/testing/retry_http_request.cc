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

#include "google/cloud/storage/testing/retry_http_request.h"
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

StatusOr<internal::HttpResponse> RetryHttpRequest(
    std::function<internal::CurlRequest()> const& factory) {
  StatusOr<internal::HttpResponse> response;
  for (int i = 0; i != 3; ++i) {
    response = factory().MakeRequest({});
    if (response) return response;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return response;
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
