// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/bigtable_round_robin.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include "google/bigtable/v2/bigtable.pb.h"
#include <gmock/gmock.h>
#include <grpcpp/client_context.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::StatusIs;
using ::testing::InSequence;
using ::testing::Return;

// All the tests have nearly identical structure. They create 3 mocks, setup
// each mock to receive 2 calls of some function, then call the
// BigtableRoundRobin version of that function 6 times.  The mocks are setup to
// return errors because it is simpler to do so than return the specific
// "success" type.

auto constexpr kMockCount = 3;
auto constexpr kRepeats = 2;

std::vector<std::shared_ptr<MockBigtableStub>> MakeMocks() {
  std::vector<std::shared_ptr<MockBigtableStub>> mocks(kMockCount);
  std::generate(mocks.begin(), mocks.end(),
                [] { return std::make_shared<MockBigtableStub>(); });
  return mocks;
}

std::vector<std::shared_ptr<BigtableStub>> AsPlainStubs(
    std::vector<std::shared_ptr<MockBigtableStub>> mocks) {
  return std::vector<std::shared_ptr<BigtableStub>>(mocks.begin(), mocks.end());
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::ReadRowsResponse>>
MakeReadRowsStream(std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const&) {
  using ErrorStream = ::google::cloud::internal::StreamingReadRpcError<
      google::bigtable::v2::ReadRowsResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::SampleRowKeysResponse>>
MakeSampleRowKeysStream(std::unique_ptr<grpc::ClientContext>,
                        google::bigtable::v2::SampleRowKeysRequest const&) {
  using ErrorStream = ::google::cloud::internal::StreamingReadRpcError<
      google::bigtable::v2::SampleRowKeysResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::MutateRowsResponse>>
MakeMutateRowsStream(std::unique_ptr<grpc::ClientContext>,
                     google::bigtable::v2::MutateRowsRequest const&) {
  using ErrorStream = ::google::cloud::internal::StreamingReadRpcError<
      google::bigtable::v2::MutateRowsResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

TEST(BigtableRoundRobinTest, ReadRows) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ReadRows).WillOnce(MakeReadRowsStream);
    }
  }

  BigtableRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::bigtable::v2::ReadRowsRequest request;
    auto stream =
        under_test.ReadRows(absl::make_unique<grpc::ClientContext>(), request);
    auto v = stream->Read();
    ASSERT_TRUE(absl::holds_alternative<Status>(v));
    EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(BigtableRoundRobinTest, SampleRowKeys) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, SampleRowKeys).WillOnce(MakeSampleRowKeysStream);
    }
  }

  BigtableRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::bigtable::v2::SampleRowKeysRequest request;
    auto stream = under_test.SampleRowKeys(
        absl::make_unique<grpc::ClientContext>(), request);
    auto v = stream->Read();
    ASSERT_TRUE(absl::holds_alternative<Status>(v));
    EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(BigtableRoundRobinTest, MutateRow) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, MutateRow)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  BigtableRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::bigtable::v2::MutateRowRequest request;
    auto response = under_test.MutateRow(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(BigtableRoundRobinTest, MutateRows) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, MutateRows).WillOnce(MakeMutateRowsStream);
    }
  }

  BigtableRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::bigtable::v2::MutateRowsRequest request;
    auto stream = under_test.MutateRows(
        absl::make_unique<grpc::ClientContext>(), request);
    auto v = stream->Read();
    ASSERT_TRUE(absl::holds_alternative<Status>(v));
    EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(BigtableRoundRobinTest, CheckAndMutateRow) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, CheckAndMutateRow)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  BigtableRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::bigtable::v2::CheckAndMutateRowRequest request;
    auto response = under_test.CheckAndMutateRow(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(BigtableRoundRobinTest, PingAndWarm) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, PingAndWarm)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  BigtableRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::bigtable::v2::PingAndWarmRequest request;
    auto response = under_test.PingAndWarm(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(BigtableRoundRobinTest, ReadModifyWriteRow) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ReadModifyWriteRow)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  BigtableRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::bigtable::v2::ReadModifyWriteRowRequest request;
    auto response = under_test.ReadModifyWriteRow(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(BigtableRoundRobinTest, AsyncMutateRow) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncMutateRow)
          .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                       google::bigtable::v2::MutateRowRequest const&) {
            return make_ready_future<
                StatusOr<google::bigtable::v2::MutateRowResponse>>(
                Status(StatusCode::kPermissionDenied, "uh-oh"));
          });
    }
  }

  CompletionQueue cq;
  BigtableRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::bigtable::v2::MutateRowRequest request;
    auto response = under_test.AsyncMutateRow(
        cq, absl::make_unique<grpc::ClientContext>(), request);
    EXPECT_THAT(response.get(), StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(BigtableRoundRobinTest, AsyncCheckAndMutateRow) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncCheckAndMutateRow)
          .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                       google::bigtable::v2::CheckAndMutateRowRequest const&) {
            return make_ready_future<
                StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>(
                Status(StatusCode::kPermissionDenied, "uh-oh"));
          });
    }
  }

  CompletionQueue cq;
  BigtableRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::bigtable::v2::CheckAndMutateRowRequest request;
    auto response = under_test.AsyncCheckAndMutateRow(
        cq, absl::make_unique<grpc::ClientContext>(), request);
    EXPECT_THAT(response.get(), StatusIs(StatusCode::kPermissionDenied));
  }
}
TEST(BigtableRoundRobinTest, AsyncReadModifyWriteRow) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncReadModifyWriteRow)
          .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                       google::bigtable::v2::ReadModifyWriteRowRequest const&) {
            return make_ready_future<
                StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>(
                Status(StatusCode::kPermissionDenied, "uh-oh"));
          });
    }
  }

  CompletionQueue cq;
  BigtableRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::bigtable::v2::ReadModifyWriteRowRequest request;
    auto response = under_test.AsyncReadModifyWriteRow(
        cq, absl::make_unique<grpc::ClientContext>(), request);
    EXPECT_THAT(response.get(), StatusIs(StatusCode::kPermissionDenied));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
