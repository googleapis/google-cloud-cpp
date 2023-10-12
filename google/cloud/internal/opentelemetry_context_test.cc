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
#include <gmock/gmock.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/trace/scope.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(OTelContext, PushPopOTelContext) {
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());

  auto s1 = opentelemetry::trace::Scope(MakeSpan("s1"));
  auto c1 = opentelemetry::context::RuntimeContext::GetCurrent();

  PushOTelContext();
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1));

  {
    auto s2 = opentelemetry::trace::Scope(MakeSpan("s2"));
    auto c2 = opentelemetry::context::RuntimeContext::GetCurrent();

    PushOTelContext();
    EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));

    PopOTelContext();
    EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1));
  }

  PopOTelContext();
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());
}

TEST(OTelContext, PopOTelContextDropsStack) {
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());

  auto s1 = opentelemetry::trace::Scope(MakeSpan("s1"));
  auto c1 = opentelemetry::context::RuntimeContext::GetCurrent();

  PushOTelContext();
  PushOTelContext();
  PushOTelContext();
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c1, c1));

  PopOTelContext();
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());
}

TEST(OTelContext, AttachDetachOTelContext) {
  auto c1 = opentelemetry::context::Context{"name", "c1"};
  AttachOTelContext(c1);
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1));
  EXPECT_EQ(c1, opentelemetry::context::RuntimeContext::GetCurrent());

  auto c2 = opentelemetry::context::Context{"name", "c2"};
  AttachOTelContext(c2);
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));
  EXPECT_EQ(c2, opentelemetry::context::RuntimeContext::GetCurrent());

  DetachOTelContext(c2);
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1));
  EXPECT_EQ(c1, opentelemetry::context::RuntimeContext::GetCurrent());

  DetachOTelContext(c1);
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());
  EXPECT_EQ(opentelemetry::context::Context{},
            opentelemetry::context::RuntimeContext::GetCurrent());
}

TEST(OTelContext, DetachOTelContextDropsStack) {
  auto c1 = opentelemetry::context::Context{"name", "c1"};
  AttachOTelContext(c1);
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1));
  EXPECT_EQ(c1, opentelemetry::context::RuntimeContext::GetCurrent());

  auto c2 = opentelemetry::context::Context{"name", "c2"};
  AttachOTelContext(c2);
  EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));
  EXPECT_EQ(c2, opentelemetry::context::RuntimeContext::GetCurrent());

  DetachOTelContext(c1);
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());
  EXPECT_EQ(opentelemetry::context::Context{},
            opentelemetry::context::RuntimeContext::GetCurrent());
}

TEST(OTelContext, ScopedOTelContext) {
  auto c1 = opentelemetry::context::Context{"name", "c1"};
  auto c2 = opentelemetry::context::Context{"name", "c2"};

  {
    OTelContext oc = {c1, c2};
    ScopedOTelContext scope(oc);
    EXPECT_THAT(CurrentOTelContext(), ElementsAre(c1, c2));
  }
  EXPECT_THAT(CurrentOTelContext(), IsEmpty());
}

TEST(OTelContext, ThreadLocalStorage) {
  auto c1 = opentelemetry::context::Context{"name", "c1"};
  auto c2 = opentelemetry::context::Context{"name", "c2"};

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
