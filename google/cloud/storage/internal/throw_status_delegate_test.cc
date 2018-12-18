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

#include "google/cloud/storage/internal/throw_status_delegate.h"
#include "google/cloud/testing_util/expect_exception.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
TEST(ThrowStatusDelegateTest, TestThrow) {
  testing_util::ExpectException<RuntimeStatusError>(
      [&] {
        Status status(404, "NOT FOUND", "oh noes!");
        ThrowStatus(std::move(status));
      },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(404, ex.status().status_code());
        EXPECT_EQ("NOT FOUND", ex.status().error_message());
        EXPECT_EQ("oh noes!", ex.status().error_details());
      },
      "Aborting because exceptions are disabled: "
      "NOT FOUND \\[404\\], details=oh noes!");
}
}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
