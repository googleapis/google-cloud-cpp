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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_HTTP_REQUEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_HTTP_REQUEST_H

#include "google/cloud/internal/rest_request.h"
#include "google/cloud/status_or.h"
#include <functional>
#include <string>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

/// Avoid flakes in integration tests, retry failures 3 times.
StatusOr<std::string> RetryHttpGet(
    std::string const& url,
    std::function<rest_internal::RestRequest()> const& factory);

/// Avoid flakes in integration tests, retry failures 3 times.
StatusOr<std::string> RetryHttpPut(
    std::string const& url,
    std::function<rest_internal::RestRequest()> const& factory,
    std::string const& payload);

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RETRY_HTTP_REQUEST_H
