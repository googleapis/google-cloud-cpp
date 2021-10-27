// Copyright 2021 Google LLC
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

#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(CaptureVersions, Default) {
  google::cloud::Options options;
  ASSERT_TRUE(options.has<internal::ApiClientHeaderOption>());
  ASSERT_EQ(options.get<internal::ApiClientHeaderOption>(),
            internal::ApplicationApiClientHeader());
#if GOOGLE_CLOUD_CPP_TEST_DIFFERENT_CPP_VERSIONS
  EXPECT_NE(internal::ApplicationLanguageVersion(),
            internal::LanguageVersion());
  EXPECT_NE(internal::ApplicationApiClientHeader(),
            internal::ApiClientHeader());
#endif  // GOOGLE_CLOUD_CPP_TEST_DIFFERENT_CXX_VERSIONS
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
