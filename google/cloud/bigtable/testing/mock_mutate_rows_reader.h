// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_MUTATE_ROWS_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_MUTATE_ROWS_READER_H

#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {
using MockMutateRowsReader =
    MockResponseReader<google::bigtable::v2::MutateRowsResponse,
                       google::bigtable::v2::MutateRowsRequest>;

// An alternative name could be `MockAsyncMutateRowReader`, but that is too
// small a difference with `MockAsyncMutateRowsReader` (note the `s` is `Rows`)
// so we prefer to call them by the higher-level API (Apply vs. BulkApply).
using MockAsyncApplyReader =
    ::google::cloud::testing_util::MockAsyncResponseReader<
        google::bigtable::v2::MutateRowResponse>;

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_MUTATE_ROWS_READER_H
