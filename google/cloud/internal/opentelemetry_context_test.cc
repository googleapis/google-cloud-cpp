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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/opentelemetry_context.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/trace/scope.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

opentelemetry::context::Context MakeNewContext() {
  // Note that `MakeNewContext() != MakeNewContext()`. This would not be true if
  // we returned a default constructed `Context`.
  return opentelemetry::context::Context("key", true);
}

class OTelContextTest : public ::testing::Test {
 public:
  // The code under test uses static storage. Ensure that each test starts
  // with a clean slate.
  OTelContextTest() {
    auto v = CurrentOTelContext();
    for (auto it = v.rbegin(); it != v.rend(); ++it) DetachOTelContext(*it);
  }
};

TEST_F(OTelContextTest, OTelScope) {
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());

  {
    OTelScope scope1(MakeSpan("s1"));
    auto c1 = opentelemetry::context::RuntimeContext::GetCurrent();
    EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1));

    {
      OTelScope scope2(MakeSpan("s2"));
      auto c2 = opentelemetry::context::RuntimeContext::GetCurrent();
      EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));
    }

    EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1));
  }

  EXPECT_THAT(CurrentOTelContext(), IsEmpty());
}

TEST_F(OTelContextTest, AttachDetachOTelContext) {
  auto c1 = MakeNewContext();
  AttachOTelContext(c1);
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1));
  EXPECT_EQ(c1, opentelemetry::context::RuntimeContext::GetCurrent());

  auto c2 = MakeNewContext();
  AttachOTelContext(c2);
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));
  EXPECT_EQ(c2, opentelemetry::context::RuntimeContext::GetCurrent());

  DetachOTelContext(c2);
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1));
  EXPECT_EQ(c1, opentelemetry::context::RuntimeContext::GetCurrent());

  DetachOTelContext(c1);
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());
  EXPECT_EQ(opentelemetry::context::Context(),
            opentelemetry::context::RuntimeContext::GetCurrent());
}

TEST_F(OTelContextTest, Scope) {
  auto c1 = MakeNewContext();
  auto c2 = MakeNewContext();

  {
    OTelContext oc = {c1, c2};
    ScopedOTelContext scope(oc);
    EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));
  }
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());
}

TEST_F(OTelContextTest, ScopeHandlesEmpty) {
  ScopedOTelContext scope({});
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());
}

TEST_F(OTelContextTest, ScopeNoopWhenContextIsAlreadyActive) {
  auto c1 = MakeNewContext();
  auto c2 = MakeNewContext();

  OTelContext oc = {c1, c2};
  ScopedOTelContext s1(oc);
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));

  {
    // Simulate the case where a future is satisfied immediately.
    ScopedOTelContext s2(oc);
    EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));
  }
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));
}

TEST_F(OTelContextTest, OTelScopeCanBeDetachedBeforeObjectIsDestroyed) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  OTelScope scope(span);
  EXPECT_TRUE(ThereIsAnActiveSpan());
  ASSERT_THAT(CurrentOTelContext(), Not(IsEmpty()));

  DetachOTelContext(CurrentOTelContext().back());
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());
  EXPECT_FALSE(ThereIsAnActiveSpan());
}

TEST_F(OTelContextTest, OTelScopeDestructorOnlyDetachesItsOwnContext) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto parent = MakeSpan("parent");
  auto span = MakeSpan("span");

  OTelScope parent_scope(parent);
  ASSERT_THAT(CurrentOTelContext(), Not(IsEmpty()));
  auto oc = CurrentOTelContext().back();
  {
    OTelScope scope(span);

    // Detach the context associated with `scope`.
    ASSERT_THAT(CurrentOTelContext(), ElementsAre(oc, _));
    DetachOTelContext(CurrentOTelContext().back());
    EXPECT_THAT(CurrentOTelContext(), ElementsAre(oc));
  }
  // `scope` has been destroyed. None of this should affect `parent_scope`,
  // which is associated with `oc`.
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(oc));
}

TEST_F(OTelContextTest, ThreadLocalStorage) {
  auto c1 = MakeNewContext();
  auto c2 = MakeNewContext();

  OTelContext oc = {c1, c2};
  ScopedOTelContext scope(oc);

  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));

  std::thread t([] { EXPECT_THAT(CurrentOTelContext(), IsEmpty()); });
  t.join();

  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
