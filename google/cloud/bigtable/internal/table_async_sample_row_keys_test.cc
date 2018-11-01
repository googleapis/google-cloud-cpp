// Copyright 2018 Google LLC
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
#include "google/cloud/bigtable/testing/internal_table_test_fixture.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_sample_row_keys_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <typeinfo>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {
namespace {

namespace bt = ::google::cloud::bigtable;
namespace btproto = google::bigtable::v2;
using namespace google::cloud::testing_util::chrono_literals;
using namespace ::testing;

class NoexTableAsyncSampleRowKeysTest
    : public bigtable::testing::internal::TableTestFixture {};

TEST_F(NoexTableAsyncSampleRowKeysTest, DefaultParameterTest) {
  // This test checks a successful call with 2 responses.

  using bigtable::testing::MockClientAsyncReaderInterface;

  MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>;
  std::unique_ptr<
      MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>
      reader_deleter1(reader1);
  EXPECT_CALL(*reader1, Read(_, _))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r, void*) {
        {
          r->set_row_key("foo");
          r->set_offset_bytes(11);
        }
      }))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r, void*) {
        {
          r->set_row_key("bar");
          r->set_offset_bytes(22);
        }
      }))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r, void*) {}));

  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncSampleRowKeys(_, _, _, _))
      .WillOnce(
          Invoke([&reader_deleter1](grpc::ClientContext*,
                                    btproto::SampleRowKeysRequest const& r,
                                    grpc::CompletionQueue*, void*) {
            return std::move(reader_deleter1);
          }));

  auto impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(impl);

  bool finished = false;
  table_.AsyncSampleRowKeys(
      cq, [&finished](CompletionQueue& cq, std::vector<RowKeySample>& samples,
                      grpc::Status& status) {
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(2U, samples.size());
        EXPECT_EQ("foo", samples[0].row_key);
        EXPECT_EQ(11, samples[0].offset_bytes);
        EXPECT_EQ("bar", samples[1].row_key);
        EXPECT_EQ(22, samples[1].offset_bytes);
        finished = true;
      });

  using bigtable::AsyncOperation;
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 1 read
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 2 read
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  EXPECT_FALSE(finished);
  impl->SimulateCompletion(cq, true);
  EXPECT_TRUE(finished);
}

TEST_F(NoexTableAsyncSampleRowKeysTest, RetryWorks) {
  // This test checks if a failed call is retried and whether partially
  // accumulated results are dropped.

  using bigtable::testing::MockClientAsyncReaderInterface;

  MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>;
  std::unique_ptr<
      MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>
      reader_deleter1(reader1);
  EXPECT_CALL(*reader1, Read(_, _))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r, void*) {
        {
          r->set_row_key("foo");
          r->set_offset_bytes(11);
        }
      }))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r, void*) {}));

  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "mocked-status");
      }));

  MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>* reader2 =
      new MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>;
  std::unique_ptr<
      MockClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>
      reader_deleter2(reader2);
  EXPECT_CALL(*reader2, Read(_, _))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r, void*) {
        {
          r->set_row_key("foo");
          r->set_offset_bytes(11);
        }
      }))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r, void*) {
        {
          r->set_row_key("bar");
          r->set_offset_bytes(22);
        }
      }))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r, void*) {}));

  EXPECT_CALL(*reader2, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncSampleRowKeys(_, _, _, _))
      .WillOnce(
          Invoke([&reader_deleter1](grpc::ClientContext*,
                                    btproto::SampleRowKeysRequest const& r,
                                    grpc::CompletionQueue*, void*) {
            return std::move(reader_deleter1);
          }))
      .WillOnce(
          Invoke([&reader_deleter2](grpc::ClientContext*,
                                    btproto::SampleRowKeysRequest const& r,
                                    grpc::CompletionQueue*, void*) {
            return std::move(reader_deleter2);
          }));

  auto impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(impl);

  bool finished = false;
  table_.AsyncSampleRowKeys(
      cq, [&finished](CompletionQueue& cq, std::vector<RowKeySample>& samples,
                      grpc::Status& status) {
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(2U, samples.size());
        EXPECT_EQ("foo", samples[0].row_key);
        EXPECT_EQ(11, samples[0].offset_bytes);
        EXPECT_EQ("bar", samples[1].row_key);
        EXPECT_EQ(22, samples[1].offset_bytes);
        finished = true;
      });

  using bigtable::AsyncOperation;
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 1 read
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  impl->SimulateCompletion(cq, true);
  // finished, scheduled timer
  impl->SimulateCompletion(cq, true);
  // timer finished, retry
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 1 read
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 2 read
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  EXPECT_FALSE(finished);
  impl->SimulateCompletion(cq, true);
  EXPECT_TRUE(finished);
}

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
