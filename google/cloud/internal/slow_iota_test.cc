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

#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/random.h"
#include "absl/types/variant.h"
#include <gmock/gmock.h>
#include <deque>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

/**
 * @file
 *
 * Prototype source<T, E>.
 */

/**
 * Simulate a source of numbers with unpredictable delays, much like
 * a Pub/Sub subscription (if Pub/Sub only generated integers).
 */
class SlowIota {
 public:
  template <typename Duration>
  SlowIota(CompletionQueue cq, int count, Duration const& period)
      : cq_(std::move(cq)),
        period_(std::chrono::duration_cast<std::chrono::microseconds>(period)),
        counter_limit_(count) {}

  SlowIota(SlowIota&& rhs) noexcept = default;

  //@{
  /**
   * @name Meet the source<int> concept.
   */

  /// Define the type of events in the source.
  using value_type = int;
  using error_type = google::cloud::Status;

  /// Get the next event, only one such call allowed at a time.
  future<absl::variant<int, google::cloud::Status>> next();
  //@}

 private:
  CompletionQueue cq_;
  std::chrono::microseconds period_;
  int counter_limit_;
  int counter_ = 0;
};

using ::testing::ElementsAre;

TEST(SlowIota, Blocking) {
  // Create a completion queue and run some background threads for it.
  internal::AutomaticallyCreatedBackgroundThreads pool;

  // Create an source that "slowly" generates integers from 0 to N.
  auto constexpr kTestCount = 10;
  auto constexpr kTestPeriod = std::chrono::microseconds(10);
  SlowIota iota(pool.cq(), kTestCount, kTestPeriod);

  // Retrieve the results blocking on each, yuck.
  std::vector<int> results;
  for (auto v = iota.next().get(); v.index() == 0; v = iota.next().get()) {
    results.push_back(absl::get<0>(v));
  }
  EXPECT_THAT(results, ElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9));

  pool.Shutdown();
}

TEST(SlowIota, Background) {
  // Create a completion queue and run some background threads for it.
  internal::AutomaticallyCreatedBackgroundThreads pool;

  // Create an source that "slowly" generates integers from 0 to N.
  auto constexpr kTestCount = 10;
  auto constexpr kTestPeriod = std::chrono::microseconds(10);
  SlowIota iota(pool.cq(), kTestCount, kTestPeriod);

  // TODO(#...) - this moves to a generic, standalone function.
  auto background_accumulate = [](SlowIota iota) {
    struct Holder {
      SlowIota source;
      std::vector<int> results;

      void start(promise<absl::variant<std::vector<int>, Status>> done) {
        struct OnNext {
          Holder* self;
          promise<absl::variant<std::vector<int>, Status>> done;
          void operator()(future<absl::variant<int, Status>> f) {
            self->on_next(f.get(), std::move(done));
          }
        };
        source.next().then(OnNext{this, std::move(done)});
      }
      void on_next(absl::variant<int, Status> v,
                   promise<absl::variant<std::vector<int>, Status>> done) {
        struct Visitor {
          Holder* self;
          promise<absl::variant<std::vector<int>, Status>> done;
          void operator()(int v) {
            self->results.push_back(v);
            self->start(std::move(done));
          }
          void operator()(Status s) {
            if (s.ok()) {
              done.set_value(std::move(self->results));
            } else {
              done.set_value(std::move(s));
            }
          }
        };
        absl::visit(Visitor{this, std::move(done)}, v);
      }
    };

    auto holder = std::make_shared<Holder>(Holder{std::move(iota), {}});
    promise<absl::variant<std::vector<int>, Status>> done;
    auto f = done.get_future();
    holder->start(std::move(done));
    // This is an idiom to extend the lifetime of `holder` until the (returned)
    // future is satisfied.  The (returned) future owns the lambda, which owns
    // `holder`. When the returned future is satisfied the lambda is called,
    // then deleted, and that deletes `holder`.
    return f.then([holder](future<absl::variant<std::vector<int>, Status>> f) {
      return f.get();
    });
  };

  auto results = background_accumulate(std::move(iota)).get();
  ASSERT_EQ(results.index(), 0) << ", status=" << absl::get<1>(results);
  EXPECT_THAT(absl::get<0>(results), ElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9));

  pool.Shutdown();
}

future<absl::variant<int, google::cloud::Status>> SlowIota::next() {
  using R = absl::variant<int, google::cloud::Status>;
  if (counter_ >= counter_limit_) {
    ++counter_;
    return make_ready_future(R{Status{}});
  }

  auto counter = counter_;
  ++counter_;
  return cq_.MakeRelativeTimer(period_).then(
      [counter](future<StatusOr<std::chrono::system_clock::time_point>> f) {
        auto tp = f.get();
        if (!tp) return R{std::move(tp).status()};
        return R{counter};
      });
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
