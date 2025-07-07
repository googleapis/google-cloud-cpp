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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_MUTATE_ROWS_LIMITER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_MUTATE_ROWS_LIMITER_H

#include "google/cloud/bigtable/internal/mutate_rows_limiter.h"
#include "google/bigtable/v2/bigtable.pb.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

class MockMutateRowsLimiter : public bigtable_internal::MutateRowsLimiter {
 public:
  MOCK_METHOD(void, Acquire, (), (override));
  MOCK_METHOD(future<void>, AsyncAcquire, (), (override));
  MOCK_METHOD(void, Update, (google::bigtable::v2::MutateRowsResponse const&),
              (override));
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_MUTATE_ROWS_LIMITER_H
