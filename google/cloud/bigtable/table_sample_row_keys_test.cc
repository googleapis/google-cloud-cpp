// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/mock_sample_row_keys_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <typeinfo>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::chrono_literals::operator"" _us;
using ::google::cloud::bigtable::testing::MockSampleRowKeysReader;
using ::testing::Return;
using ::testing::Unused;

class TableSampleRowKeysTest
    : public ::google::cloud::bigtable::testing::TableTestFixture {
 public:
  TableSampleRowKeysTest() : TableTestFixture(CompletionQueue{}) {}
};

/// @test Verify that Table::SampleRows() works.
TEST_F(TableSampleRowKeysTest, SampleRowKeysTest) {
  namespace btproto = ::google::bigtable::v2;

  EXPECT_CALL(*client_, SampleRowKeys).WillOnce([](Unused, Unused) {
    auto reader = absl::make_unique<MockSampleRowKeysReader>(
        "google.bigtable.v2.Bigtable.SampleRowKeys");
    EXPECT_CALL(*reader, Read)
        .WillOnce([](btproto::SampleRowKeysResponse* r) {
          {
            r->set_row_key("test1");
            r->set_offset_bytes(11);
          }
          return true;
        })
        .WillOnce(Return(false));
    EXPECT_CALL(*reader, Finish).WillOnce(Return(grpc::Status::OK));
    return reader;
  });
  auto result = table_.SampleRows();
  ASSERT_STATUS_OK(result);
  auto it = result->begin();
  EXPECT_NE(it, result->end());
  EXPECT_EQ(it->row_key, "test1");
  EXPECT_EQ(it->offset_bytes, 11);
  EXPECT_EQ(++it, result->end());
}

/// @test Verify that Table::SampleRows() retries when unavailable.
TEST_F(TableSampleRowKeysTest, SampleRowKeysRetryTest) {
  namespace btproto = ::google::bigtable::v2;

  EXPECT_CALL(*client_, SampleRowKeys)
      .WillOnce([](Unused, Unused) {
        auto reader = absl::make_unique<MockSampleRowKeysReader>(
            "google.bigtable.v2.Bigtable.SampleRowKeys");
        EXPECT_CALL(*reader, Read)
            .WillOnce([](btproto::SampleRowKeysResponse* r) {
              {
                r->set_row_key("test1");
                r->set_offset_bytes(11);
              }
              return true;
            })
            .WillOnce(Return(false));

        EXPECT_CALL(*reader, Finish())
            .WillOnce(Return(
                grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));
        return reader;
      })
      .WillOnce([](Unused, Unused) {
        auto reader_retry = absl::make_unique<MockSampleRowKeysReader>(
            "google.bigtable.v2.Bigtable.SampleRowKeys");
        EXPECT_CALL(*reader_retry, Read)
            .WillOnce([](btproto::SampleRowKeysResponse* r) {
              {
                r->set_row_key("test2");
                r->set_offset_bytes(123);
              }
              return true;
            })
            .WillOnce([](btproto::SampleRowKeysResponse* r) {
              {
                r->set_row_key("test3");
                r->set_offset_bytes(1234);
              }
              return true;
            })
            .WillOnce(Return(false));

        EXPECT_CALL(*reader_retry, Finish()).WillOnce(Return(grpc::Status::OK));
        return reader_retry;
      });

  auto results = table_.SampleRows();
  ASSERT_STATUS_OK(results);

  auto it = results->begin();
  EXPECT_NE(it, results->end());
  EXPECT_EQ("test2", it->row_key);
  ++it;
  EXPECT_NE(it, results->end());
  EXPECT_EQ("test3", it->row_key);
  EXPECT_EQ(++it, results->end());
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
/// @test Verify that Table::SampleRows() reports correctly on too many errors.
TEST_F(TableSampleRowKeysTest, TooManyFailures) {
  namespace btproto = ::google::bigtable::v2;

  // Create a table with specific policies so we can test the behavior
  // without having to depend on timers expiring.  In this case tolerate only
  // 3 failures.
  Table custom_table(
      client_, "foo_table",
      // Configure the Table to stop at 3 failures.
      LimitedErrorCountRetryPolicy(2),
      // Use much shorter backoff than the default to test faster.
      ExponentialBackoffPolicy(10_us, 40_us), SafeIdempotentMutationPolicy());

  // Setup the mocks to fail more than 3 times.
  auto create_cancelled_stream = [](Unused, Unused) {
    auto stream = absl::make_unique<MockSampleRowKeysReader>(
        "google.bigtable.v2.Bigtable.SampleRowKeys");
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish)
        .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));
    return stream;
  };

  EXPECT_CALL(*client_, SampleRowKeys)
      .WillOnce([](Unused, Unused) {
        auto r1 = absl::make_unique<MockSampleRowKeysReader>(
            "google.bigtable.v2.Bigtable.SampleRowKeys");
        EXPECT_CALL(*r1, Read)
            .WillOnce([](btproto::SampleRowKeysResponse* r) {
              {
                r->set_row_key("test1");
                r->set_offset_bytes(11);
              }
              return true;
            })
            .WillOnce(Return(false));
        EXPECT_CALL(*r1, Finish)
            .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));
        return r1;
      })
      .WillOnce(create_cancelled_stream)
      .WillOnce(create_cancelled_stream);

  EXPECT_FALSE(custom_table.SampleRows());
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
