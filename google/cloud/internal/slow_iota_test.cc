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
#include "google/cloud/internal/source_accumulators.h"
#include "google/cloud/internal/source_builder.h"
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
        counter_limit_(count),
        flow_control_(1) {}

  SlowIota(SlowIota&&) noexcept = default;

  //@{
  /**
   * @name Meet the source<int> concept.
   */

  /// Define the type of events in the source.
  using value_type = int;
  using error_type = google::cloud::Status;

  future<ReadyToken> ready() { return flow_control_.Acquire(); }

  /// Get the next event, only one such call allowed at a time.
  future<absl::variant<int, google::cloud::Status>> next(ReadyToken token);
  //@}

 private:
  CompletionQueue cq_;
  std::chrono::microseconds period_;
  int counter_limit_;
  int counter_ = 0;

  internal::ReadyTokenFlowControl flow_control_;
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
  auto next = [&iota] { return iota.next(iota.ready().get()).get(); };
  for (auto v = next(); v.index() == 0; v = next()) {
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

  auto results = MakeSourceBuilder(std::move(iota))
                     .Accumulate<AccumulateAllEvents>()
                     .get();
  ASSERT_EQ(results.index(), 0) << ", status=" << absl::get<1>(results);
  EXPECT_THAT(absl::get<0>(results), ElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9));

  pool.Shutdown();
}

future<absl::variant<int, google::cloud::Status>> SlowIota::next(
    ReadyToken token) {
  if (!flow_control_.Release(std::move(token))) {
    google::cloud::internal::ThrowLogicError("invalid token in flow control");
  }
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
