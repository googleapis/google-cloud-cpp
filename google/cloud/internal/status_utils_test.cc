// Copyright 2024 Google LLC
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

#include "google/cloud/internal/status_utils.h"
#include "google/cloud/status.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(IsClientTest, OriginatesFromClient) {
  Status cases[] = {
      Status(StatusCode::kCancelled, "cancelled + contains origin metadata",
             ErrorInfo("test-only-reasons", "test-only-domain",
                       {{"gl-cpp.error.origin", "client"}})),
      Status(StatusCode::kUnknown,
             "unknown + contains origin metadata + other metadata",
             ErrorInfo("test-only-reasons", "test-only-domain",
                       {
                           {"some-other-key", "random-value"},
                           {"gl-cpp.error.origin", "client"},
                       }))};

  for (auto const& status : cases) {
    SCOPED_TRACE("Testing status: " + StatusCodeToString(status.code()) +
                 " - " + status.message());
    EXPECT_EQ(IsClient(status), ErrorOrigin::kClient);
  }
}

TEST(IsClientTest, DoesNotOriginateFromClient) {
  Status cases[] = {
      Status(StatusCode::kAborted, "no metadata"),
      Status(StatusCode::kCancelled, "incorrect origin value",
             ErrorInfo("test-only-reasons", "test-only-domain",
                       {{"gl-cpp.error.origin", "server"}})),
      Status(StatusCode::kOk, "incorrect origin value with client prefix",
             ErrorInfo("test-only-reasons", "test-only-domain",
                       {{"gl-cpp.error.origin", "client-maybe"}})),
      Status(StatusCode::kUnknown, "does not contain origin value",
             ErrorInfo("test-only-reasons", "test-only-domain",
                       {
                           {"some-other-key", "random-value"},
                       })),
      Status(StatusCode::kOk, "success status + contains origin metadata",
             ErrorInfo("test-only-reasons", "test-only-domain",
                       {{"gl-cpp.error.origin", "client"}}))};

  for (auto const& status : cases) {
    SCOPED_TRACE("Testing status: " + StatusCodeToString(status.code()) +
                 " - " + status.message());
    EXPECT_EQ(IsClient(status), ErrorOrigin::kUnknown);
  }
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
