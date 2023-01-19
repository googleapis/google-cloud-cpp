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

#include "google/cloud/internal/opentelemetry_aliases.h"
#include "google/cloud/internal/opentelemetry.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

TEST(CurrentSpan, WithOpenTelemetry) {
  auto s1 = MakeSpan("s1");
  auto s2 = MakeSpan("s2");

  EXPECT_FALSE(CurrentSpan()->GetContext().IsValid());
  {
    auto scope_first = ScopedSpan(s1);
    ASSERT_EQ(CurrentSpan(), s1);
    {
      auto scope_second = ScopedSpan(s2);
      ASSERT_EQ(CurrentSpan(), s2);
    }
    ASSERT_EQ(CurrentSpan(), s1);
  }
  EXPECT_FALSE(CurrentSpan()->GetContext().IsValid());
}

#else

TEST(CurrentSpan, WithoutOpenTelemetry) {
  Span span = CurrentSpan();
  ScopedSpan scope(span);
  EXPECT_EQ(span, nullptr);
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
