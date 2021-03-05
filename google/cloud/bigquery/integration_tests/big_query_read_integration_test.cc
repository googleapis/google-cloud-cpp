// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/big_query_read_client.gcpcxx.pb.h"
#include "google/cloud/bigquery/internal/big_query_read_stub_factory.gcpcxx.pb.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

class BigQueryReadIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    connection_options_.enable_tracing("rpc");
    retry_policy_ =
        absl::make_unique<BigQueryReadLimitedErrorCountRetryPolicy>(1);
    backoff_policy_ = absl::make_unique<ExponentialBackoffPolicy>(
        std::chrono::seconds(1), std::chrono::seconds(1), 2.0);
  }
  BigQueryReadConnectionOptions connection_options_;
  std::unique_ptr<BigQueryReadRetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  testing_util::ScopedLog log_;
};

// TODO(sdhart): add integration tests
TEST_F(BigQueryReadIntegrationTest, AlwaysSucceeds) { EXPECT_TRUE(true); }

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google
