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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_DATABASE_INTEGRATION_TEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_DATABASE_INTEGRATION_TEST_H

#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include <gtest/gtest.h>

namespace google {
namespace cloud {
/// Helper types and functions used in Cloud Spanner C++ client library tests.
namespace spanner_testing {
/// An inlined, versioned namespace for the testing helpers.
inline namespace SPANNER_CLIENT_NS {

class DatabaseIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 public:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

  static google::cloud::spanner::Database const& GetDatabase() { return *db_; }

 private:
  static google::cloud::spanner::Database* db_;
  static google::cloud::internal::DefaultPRNG* generator_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_DATABASE_INTEGRATION_TEST_H
