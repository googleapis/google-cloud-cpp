// Copyright 2020 Google LLC
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

#include "google/cloud/internal/source_builder.h"
#include "google/cloud/internal/source_accumulators.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/fake_source.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::FakeSource;
using ::testing::ElementsAre;

TEST(SourceBuilder, Simple) {
  auto transformed =
      MakeSourceBuilder(FakeSource<int, Status>({1, 2, 3, 4}, Status{}))
          .transform([](int x) { return x * 2; })
          .transform([](int x) { return std::to_string(x); })
          .build();
  std::vector<std::string> events;
  auto next = [&transformed] {
    return transformed.next(transformed.ready().get()).get();
  };
  std::vector<std::string> values;
  for (auto v = next(); v.index() == 0; v = next()) {
    values.push_back(absl::get<0>(v));
  }
  EXPECT_THAT(values, ElementsAre("2", "4", "6", "8"));
}

TEST(SourceBuilder, Accumulate) {
  auto const all_events =
      MakeSourceBuilder(FakeSource<int, Status>({1, 2, 3, 4}, Status{}))
          .transform([](int x) { return x * 2; })
          .transform([](int x) { return std::to_string(x); })
          .accumulate<AccumulateAllEvents>()
          .get();
  ASSERT_EQ(all_events.index(), 0);  // expect success
  EXPECT_THAT(absl::get<0>(all_events), ElementsAre("2", "4", "6", "8"));
}

/// A test class to show accumulate() works with more than one type.
template <typename Source>
class SumAllSourceEvents {
 public:
  using value_t = typename Source::value_type;
  using error_t = typename Source::error_type;
  using source_event_t = absl::variant<value_t, error_t>;
  using event_t = absl::variant<value_t, error_t>;

  SumAllSourceEvents(Source s, value_t initial)
      : source_(std::move(s)), value_(initial) {}

  /**
   * Start accumulating data in the background.
   *
   * The caller is responsible for ensuring this object's lifetime is long
   * enough.
   */
  future<event_t> Start() {
    promise<event_t> done;
    auto f = done.get_future();
    Schedule(std::move(done));
    return f;
  }

 private:
  void Schedule(promise<event_t> done) {
    // Simulate extended lambda captures, we want to move @p done.
    struct OnReady {
      SumAllSourceEvents* self;
      promise<event_t> done;
      void operator()(future<ReadyToken> f) {
        self->OnReady(f.get(), std::move(done));
      }
    };
    source_.ready().then(OnReady{this, std::move(done)});
  }

  void OnReady(ReadyToken token, promise<event_t> done) {
    // Simulate extended lambda captures, we want to move @p done.
    struct OnNext {
      SumAllSourceEvents* self;
      promise<event_t> done;
      void operator()(future<source_event_t> f) {
        self->OnNext(f.get(), std::move(done));
      }
    };
    source_.next(std::move(token)).then(OnNext{this, std::move(done)});
  }

  void OnNext(source_event_t v, promise<event_t> done) {
    struct Visitor {
      SumAllSourceEvents* self;
      promise<event_t> done;

      void operator()(value_t v) {
        self->value_ += v;
        self->Schedule(std::move(done));
      }
      void operator()(error_t s) {
        if (s.ok()) {
          done.set_value(std::move(self->value_));
        } else {
          done.set_value(std::move(s));
        }
      }
    };
    absl::visit(Visitor{this, std::move(done)}, v);
  }

  Source source_;
  value_t value_ = {};
};

TEST(SourceBuilder, AccumulateSum) {
  auto const all_events =
      MakeSourceBuilder(FakeSource<int, Status>({1, 2, 3, 4}, Status{}))
          .transform([](int x) { return x * 2; })
          .accumulate<SumAllSourceEvents>(100)
          .get();
  ASSERT_EQ(all_events.index(), 0);  // expect success
  EXPECT_EQ(absl::get<0>(all_events), 100 + 2 + 4 + 6 + 8);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
