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

#include "bigtable/client/internal/make_unique.h"
#include "bigtable/client/table.h"
#include "bigtable/client/testing/chrono_literals.h"
#include "bigtable/client/testing/table_test_fixture.h"
#include <typeinfo>

/// Define types and functions used for this tests.
namespace {

class MockReader : public grpc::ClientReaderInterface<
                       ::google::bigtable::v2::SampleRowKeysResponse> {
 public:
  MOCK_METHOD0(WaitForInitialMetadata, void());
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t*));
  MOCK_METHOD1(Read, bool(::google::bigtable::v2::SampleRowKeysResponse*));
};

class TableSampleRowKeysTest : public bigtable::testing::TableTestFixture {};
}  // anonymous namespace

/// @test Verify that Table::SampleRows<T>() works for default parameter.
TEST_F(TableSampleRowKeysTest, DefaultParameterTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = new MockReader;
  EXPECT_CALL(*bigtable_stub_, SampleRowKeysRaw(_, _)).WillOnce(Return(reader));
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test1");
          r->set_offset_bytes(11);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));
  std::vector<bigtable::RowKeySample> result = table_.SampleRows<>();
  auto it = result.begin();
  EXPECT_NE(it, result.end());
  EXPECT_EQ(it->row_key, "test1");
  EXPECT_EQ(it->offset_bytes, 11);
  EXPECT_EQ(++it, result.end());
}

/// @test Verify that Table::SampleRows<T>() works for std::vector.
TEST_F(TableSampleRowKeysTest, SimpleVectorTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = new MockReader;
  EXPECT_CALL(*bigtable_stub_, SampleRowKeysRaw(_, _)).WillOnce(Return(reader));
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test1");
          r->set_offset_bytes(11);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));
  std::vector<bigtable::RowKeySample> result = table_.SampleRows<std::vector>();
  auto it = result.begin();
  EXPECT_NE(it, result.end());
  EXPECT_EQ(it->row_key, "test1");
  EXPECT_EQ(it->offset_bytes, 11);
  EXPECT_EQ(++it, result.end());
}

/// @test Verify that Table::SampleRows<T>() works for std::list.
TEST_F(TableSampleRowKeysTest, SimpleListTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = new MockReader;
  EXPECT_CALL(*bigtable_stub_, SampleRowKeysRaw(_, _)).WillOnce(Return(reader));
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test1");
          r->set_offset_bytes(11);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));
  std::list<bigtable::RowKeySample> result = table_.SampleRows<std::list>();
  auto it = result.begin();
  EXPECT_NE(it, result.end());
  EXPECT_EQ(it->row_key, "test1");
  EXPECT_EQ(it->offset_bytes, 11);
  EXPECT_EQ(++it, result.end());
}

TEST_F(TableSampleRowKeysTest, SampleRowKeysRetryTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = new MockReader;
  auto reader_retry = new MockReader;
  EXPECT_CALL(*bigtable_stub_, SampleRowKeysRaw(_, _))
      .WillOnce(Return(reader))
      .WillOnce(Return(reader_retry));

  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test1");
          r->set_offset_bytes(11);
        }
        return true;
      }))
      .WillOnce(Return(false));

  EXPECT_CALL(*reader, Finish())
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  EXPECT_CALL(*reader_retry, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test2");
          r->set_offset_bytes(123);
        }
        return true;
      }))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test3");
          r->set_offset_bytes(1234);
        }
        return true;
      }))
      .WillOnce(Return(false));

  EXPECT_CALL(*reader_retry, Finish()).WillOnce(Return(grpc::Status::OK));

  auto results = table_.SampleRows<std::vector>();

  auto it = results.begin();
  EXPECT_NE(it, results.end());
  EXPECT_EQ("test2", it->row_key);
  ++it;
  EXPECT_NE(it, results.end());
  EXPECT_EQ("test3", it->row_key);
  EXPECT_EQ(++it, results.end());
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
/// @test Verify that Table::sample_rows() reports correctly on too many errors.
TEST_F(TableSampleRowKeysTest, TooManyFailures) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  using namespace bigtable::chrono_literals;
  // Create a table with specific policies so we can test the behavior
  // without having to depend on timers expiring.  In this case tolerate only
  // 3 failures.
  ::bigtable::Table custom_table(
      client_, "foo_table",
      // Configure the Table to stop at 3 failures.
      ::bigtable::LimitedErrorCountRetryPolicy(2),
      // Use much shorter backoff than the default to test faster.
      ::bigtable::ExponentialBackoffPolicy(10_us, 40_us),
      ::bigtable::SafeIdempotentMutationPolicy());

  // Setup the mocks to fail more than 3 times.
  auto r1 = new MockReader;
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test1");
          r->set_offset_bytes(11);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));

  auto create_cancelled_stream = [&](grpc::ClientContext*,
                                     btproto::SampleRowKeysRequest const&) {
    auto stream = new MockReader;
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));
    return stream;
  };

  EXPECT_CALL(*bigtable_stub_, SampleRowKeysRaw(_, _))
      .WillOnce(Return(r1))
      .WillOnce(Invoke(create_cancelled_stream))
      .WillOnce(Invoke(create_cancelled_stream));

  EXPECT_THROW(custom_table.SampleRows<std::vector>(), std::runtime_error);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
