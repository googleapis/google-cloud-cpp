// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/storage_round_robin.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::StatusIs;
using ::testing::InSequence;
using ::testing::Return;

// All the tests have nearly identical structure. They create 3 mocks, setup
// each mock to receive 2 calls of some function, then call the
// StorageRoundRobin version of that function 6 times.  The mocks are setup to
// return errors because it is simpler to do so than return the specific
// "success" type.

auto constexpr kMockCount = 3;
auto constexpr kRepeats = 2;

std::vector<std::shared_ptr<MockStorageStub>> MakeMocks() {
  std::vector<std::shared_ptr<MockStorageStub>> mocks(kMockCount);
  std::generate(mocks.begin(), mocks.end(),
                [] { return std::make_shared<MockStorageStub>(); });
  return mocks;
}

std::vector<std::shared_ptr<StorageStub>> AsPlainStubs(
    std::vector<std::shared_ptr<MockStorageStub>> mocks) {
  return std::vector<std::shared_ptr<StorageStub>>(mocks.begin(), mocks.end());
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::storage::v2::ReadObjectResponse>>
MakeReadObjectStream(std::unique_ptr<grpc::ClientContext>,
                     google::storage::v2::ReadObjectRequest const&) {
  using ErrorStream = ::google::cloud::internal::StreamingReadRpcError<
      google::storage::v2::ReadObjectResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

std::unique_ptr<google::cloud::internal::StreamingWriteRpc<
    google::storage::v2::WriteObjectRequest,
    google::storage::v2::WriteObjectResponse>>
MakeInsertStream(std::unique_ptr<grpc::ClientContext>) {
  using ErrorStream = ::google::cloud::internal::StreamingWriteRpcError<
      google::storage::v2::WriteObjectRequest,
      google::storage::v2::WriteObjectResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

TEST(StorageRoundRobinTest, DeleteBucket) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DeleteBucket)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::DeleteBucketRequest request;
    auto response = under_test.DeleteBucket(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, GetBucket) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetBucket)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::GetBucketRequest request;
    auto response = under_test.GetBucket(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, CreateBucket) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, CreateBucket)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::CreateBucketRequest request;
    auto response = under_test.CreateBucket(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, ListBuckets) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListBuckets)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::ListBucketsRequest request;
    auto response = under_test.ListBuckets(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, UpdateBucket) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, UpdateBucket)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::UpdateBucketRequest request;
    auto response = under_test.UpdateBucket(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, ComposeObject) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ComposeObject)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::ComposeObjectRequest request;
    auto response = under_test.ComposeObject(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, DeleteObject) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DeleteObject)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::DeleteObjectRequest request;
    auto response = under_test.DeleteObject(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, GetObject) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetObject)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::GetObjectRequest request;
    auto response = under_test.GetObject(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, ReadObject) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ReadObject).WillOnce(MakeReadObjectStream);
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::ReadObjectRequest request;
    auto response = under_test.ReadObject(
        absl::make_unique<grpc::ClientContext>(), request);
    auto v = response->Read();
    ASSERT_TRUE(absl::holds_alternative<Status>(v));
    EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, UpdateObject) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, UpdateObject)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::UpdateObjectRequest request;
    auto response = under_test.UpdateObject(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, WriteObject) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, WriteObject).WillOnce(MakeInsertStream);
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto response =
        under_test.WriteObject(absl::make_unique<grpc::ClientContext>());
    EXPECT_THAT(response->Close(), StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, ListObjects) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListObjects)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::ListObjectsRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.ListObjects(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, RewriteObject) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, RewriteObject)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::RewriteObjectRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.RewriteObject(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, StartResumableWrite) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, StartResumableWrite)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::StartResumableWriteRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.StartResumableWrite(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, QueryWriteStatus) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, QueryWriteStatus)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::QueryWriteStatusRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.QueryWriteStatus(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, GetServiceAccount) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetServiceAccount)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::GetServiceAccountRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.GetServiceAccount(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
