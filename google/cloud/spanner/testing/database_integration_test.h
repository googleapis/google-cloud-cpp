// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_DATABASE_INTEGRATION_TEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_DATABASE_INTEGRATION_TEST_H

#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A `testing::Test` that:
 *   - creates (for all tests in the suite) a randomly-named database,
 *     in a randomly-chosen instance,
 *   - populates the database with some useful tables, and
 *   - flushes the `google::cloud::LogSink` when a test completes with
 *     a failure (see `testing_util::IntegrationTest`).
 */
class DatabaseIntegrationTest : public testing_util::IntegrationTest {
 protected:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

  static spanner::Database const& GetDatabase() { return *db_; }
  static bool UsingEmulator() { return emulator_; }

 private:
  static internal::DefaultPRNG* generator_;
  static spanner::Database* db_;
  static bool emulator_;
};

/**
 * Same as `DatabaseIntegrationTest`, but creates the database using
 * `DatabaseDialect::POSTGRESQL`, and with PostgreSQL-specific column
 * types.
 */
class PgDatabaseIntegrationTest : public testing_util::IntegrationTest {
 protected:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

  static spanner::Database const& GetDatabase() { return *db_; }
  static bool UsingEmulator() { return emulator_; }

 private:
  static internal::DefaultPRNG* generator_;
  static spanner::Database* db_;
  static bool emulator_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_DATABASE_INTEGRATION_TEST_H
