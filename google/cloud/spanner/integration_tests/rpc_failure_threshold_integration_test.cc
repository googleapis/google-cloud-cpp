// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <cmath>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

class RpcFailureThresholdTest
    : public ::google::cloud::testing_util::IntegrationTest {
 public:
  void SetUp() override {
    auto project_id =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id.empty());

    generator_ = google::cloud::internal::MakeDefaultPRNG();
    auto instance_id =
        spanner_testing::PickRandomInstance(generator_, project_id);
    ASSERT_STATUS_OK(instance_id);

    auto database_id = spanner_testing::RandomDatabaseName(generator_);

    db_ = absl::make_unique<Database>(project_id, *instance_id, database_id);

    std::cout << "Creating database [" << database_id << "] and table "
              << std::flush;
    DatabaseAdminClient admin_client;
    auto database_future =
        admin_client.CreateDatabase(*db_, {R"""(CREATE TABLE Singers (
                                SingerId   INT64 NOT NULL,
                                FirstName  STRING(1024),
                                LastName   STRING(1024)
                             ) PRIMARY KEY (SingerId))"""});
    int i = 0;
    int const timeout = 120;
    while (++i < timeout) {
      auto status = database_future.wait_for(std::chrono::seconds(1));
      if (status == std::future_status::ready) break;
      std::cout << '.' << std::flush;
    }
    if (i >= timeout) {
      std::cout << " TIMEOUT\n";
      FAIL();
    }
    auto database = database_future.get();
    ASSERT_STATUS_OK(database);
    std::cout << " DONE\n";
  }

  void TearDown() override {
    if (!db_) {
      return;
    }
    std::cout << "Dropping database " << db_->database_id() << std::flush;
    DatabaseAdminClient admin_client;
    auto drop_status = admin_client.DropDatabase(*db_);
    std::cout << " DONE\n";
    EXPECT_STATUS_OK(drop_status);
  }

 protected:
  google::cloud::internal::DefaultPRNG generator_;
  std::unique_ptr<Database> db_;
};

struct Result {
  int number_of_successes;
  int number_of_failures;
};

/// Run a single copy of the experiment
Result RunExperiment(Database const& db, int iterations) {
  // Use a different client on each thread because we do not want to share
  // sessions, also ensure that each of these clients has a different pool of
  // gRPC channels.
  std::string pool = [] {
    std::ostringstream os;
    os << "thread-pool:" << std::this_thread::get_id();
    return std::move(os).str();
  }();

  Client client(
      MakeConnection(db, ConnectionOptions().set_channel_pool_domain(pool)));

  int number_of_successes = 0;
  int number_of_failures = 0;

  auto update_trials = [&number_of_failures,
                        &number_of_successes](Status const& status) {
    if (status.ok()) {
      ++number_of_successes;
    } else {
      ++number_of_failures;
      std::cout << status << "\n";
    }
  };

  int const report = iterations / 5;
  for (int i = 0; i != iterations; ++i) {
    if (i % report == 0) std::cout << '.' << std::flush;
    auto delete_status =
        client.Commit([&client](Transaction const& txn) -> StatusOr<Mutations> {
          auto status = client.ExecuteDml(
              txn, SqlStatement("DELETE FROM Singers WHERE true"));
          if (!status) return std::move(status).status();
          return Mutations{};
        });
    update_trials(delete_status.status());
  }

  return Result{number_of_successes, number_of_failures};
}

/**
 * @test Verify that the error rate for ExecuteDml(...DELETE) operations is
 *     within bounds.
 *
 * This program estimates the error rate for ExecuteDml() when the SQL statement
 * is 'DELETE FROM table WHERE true'. We expect this to be very low for an empty
 * table, but was high at some point.
 *
 * The program assumes that each request is a independent, identically
 * distributed, Bernoulli experiment, with an underlying probably of success
 * $p$. The program computes the 99% confidence interval for $p$ using the
 * normal approximation:
 *
 *     https://en.wikipedia.org/wiki/Binomial_proportion_confidence_interval
 *
 * If the confidence interval includes a critical threshold (0.999) then the
 * program fails, claiming that the underlying error rate may be too high.
 *
 * The number of iteration in the program was chosen to the statistical test
 * would have 0.99 power. That is, the test would miss an actual change from
 * 0.001 to 0.002 only 1% of the time.
 *
 * Note that the power (0.99) and confidence (0.01) parameters are more strict
 * that the conventional values (0.8 are 0.05). We can afford more strict tests
 * because the experiments are "cheap", so there is no reason not to.
 */
TEST_F(RpcFailureThresholdTest, ExecuteDmlDeleteErrors) {
  ASSERT_TRUE(db_);

  // We are using the approximation via a normal distribution from here:
  //   https://en.wikipedia.org/wiki/Binomial_proportion_confidence_interval
  // we select $\alpha$ as $1/1000$ because we want a very high confidence in
  // the failure rate. The approximation above requires the normal distribution
  // quantile, which we obtained using R, and the expression
  //   `qnorm(1 - (1/100.0)/2)`
  // which yields:
  double const z = 2.575829;

  // We are willing to tolerate one failure in 1,000 requests.
  double const threshold = 1 / 1000.0;

  // Detecting if the failure rate is higher than 1 / 1,000 when we observe some
  // other rate requires some power analysis to ensure the sample size is large
  // enough. Fortunately, R has a function to do precisely this:
  //
  // ```R
  // require(pwr)
  // pwr.p.test(
  //     h=ES.h(p1=0.002, p2=0.001),
  //     power=0.99, sig.level=0.01, alternative="greater")
  // proportion power calculation for binomial distribution
  // (arcsine transformation)
  //
  //      h = 0.02621646
  //      n = 31496.42
  //      sig.level = 0.01
  //      power = 0.99
  //      alternative = greater
  // ```
  //
  // This reads: you need 31,496 samples to reliably (99% power) detect an
  // effect of 2 failures per 1,000 when your null hypothesis is 1 per 1,000 and
  // you want to use a significance level of 1%.
  //
  // In practice this means running the test for about 3 minutes on a server
  // with 4 cores.

  int const desired_samples = 32000;  // slightly higher sample rate.

  auto const threads_per_core = 8;
  // GCC and Clang default capture constants, but MSVC does not, so pass the
  // constant as an argument.
  auto const number_of_threads = [](int tpc) -> unsigned {
    auto number_of_cores = std::thread::hardware_concurrency();
    return number_of_cores == 0 ? tpc : number_of_cores * tpc;
  }(threads_per_core);

  auto const iterations = static_cast<int>(desired_samples / number_of_threads);

  int number_of_successes = 0;
  int number_of_failures = 0;

  std::vector<std::future<Result>> tasks(number_of_threads);
  std::cout << "Running test " << std::flush;
  std::generate_n(tasks.begin(), number_of_threads, [this, iterations] {
    return std::async(std::launch::async, RunExperiment, *db_, iterations);
  });
  for (auto& t : tasks) {
    auto r = t.get();
    number_of_successes += r.number_of_successes;
    number_of_failures += r.number_of_failures;
  }
  std::cout << " DONE\n";

  double const number_of_trials = number_of_failures + number_of_successes;
  double const mid = number_of_successes / number_of_trials;
  double const r =
      z / number_of_trials *
      std::sqrt(number_of_failures / number_of_trials * number_of_successes);
  std::cout << "Total failures = " << number_of_failures
            << "\nTotal successes = " << number_of_successes
            << "\nTotal trials = " << number_of_trials
            << "\nEstimated 99% confidence interval for success rate is ["
            << (mid - r) << "," << (mid + r) << "]\n";

  EXPECT_GT(mid - r, 1.0 - threshold)
      << " number_of_failures=" << number_of_failures
      << ", number_of_successes=" << number_of_successes
      << ", number_of_trials=" << number_of_trials << ", mid=" << mid
      << ", r=" << r << ", range=[ " << (mid - r) << " , " << (mid + r) << "]"
      << ", kTheshold=" << threshold;
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
