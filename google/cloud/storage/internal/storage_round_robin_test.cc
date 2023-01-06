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
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/internal/async_streaming_write_rpc_impl.h"
#include "google/cloud/internal/streaming_write_rpc_impl.h"
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
using ::testing::ByMove;
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

std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<
    google::storage::v2::ReadObjectResponse>>
MakeAsyncReadObjectStream(google::cloud::CompletionQueue const&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::storage::v2::ReadObjectRequest const&) {
  using ErrorStream = ::google::cloud::internal::AsyncStreamingReadRpcError<
      google::storage::v2::ReadObjectResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

std::unique_ptr<google::cloud::internal::AsyncStreamingWriteRpc<
    google::storage::v2::WriteObjectRequest,
    google::storage::v2::WriteObjectResponse>>
MakeAsyncWriteObjectStream(google::cloud::CompletionQueue const&,
                           std::unique_ptr<grpc::ClientContext>) {
  using ErrorStream = ::google::cloud::internal::AsyncStreamingWriteRpcError<
      google::storage::v2::WriteObjectRequest,
      google::storage::v2::WriteObjectResponse>;
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

TEST(StorageRoundRobinTest, LockBucketRetentionPolicy) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, LockBucketRetentionPolicy)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::LockBucketRetentionPolicyRequest request;
    auto response = under_test.LockBucketRetentionPolicy(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, GetIamPolicy) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetIamPolicy)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::iam::v1::GetIamPolicyRequest request;
    auto response = under_test.GetIamPolicy(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, SetIamPolicy) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, SetIamPolicy)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::iam::v1::SetIamPolicyRequest request;
    auto response = under_test.SetIamPolicy(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, TestIamPermissions) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, TestIamPermissions)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::iam::v1::TestIamPermissionsRequest request;
    auto response = under_test.TestIamPermissions(context, request);
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

TEST(StorageRoundRobinTest, DeleteNotification) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DeleteNotification)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::DeleteNotificationRequest request;
    auto response = under_test.DeleteNotification(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, GetNotification) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetNotification)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::GetNotificationRequest request;
    auto response = under_test.GetNotification(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, CreateNotification) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, CreateNotification)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::CreateNotificationRequest request;
    auto response = under_test.CreateNotification(context, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, ListNotifications) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListNotifications)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::ListNotificationsRequest request;
    auto response = under_test.ListNotifications(context, request);
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

TEST(StorageRoundRobinTest, CancelResumableWrite) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, CancelResumableWrite)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::storage::v2::CancelResumableWriteRequest request;
    auto response = under_test.CancelResumableWrite(context, request);
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

TEST(StorageRoundRobinTest, CreateHmacKey) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, CreateHmacKey)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::CreateHmacKeyRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.CreateHmacKey(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, DeleteHmacKey) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DeleteHmacKey)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::DeleteHmacKeyRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.DeleteHmacKey(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, GetHmacKey) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetHmacKey)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::GetHmacKeyRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.GetHmacKey(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, ListHmacKeys) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListHmacKeys)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::ListHmacKeysRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.ListHmacKeys(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, UpdateHmacKey) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, UpdateHmacKey)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::UpdateHmacKeyRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.UpdateHmacKey(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, AsyncDeleteObject) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncDeleteObject)
          .WillOnce(Return(ByMove(make_ready_future(
              Status(StatusCode::kPermissionDenied, "uh-oh")))));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  google::cloud::CompletionQueue cq;
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::DeleteObjectRequest request;
    auto response =
        under_test
            .AsyncDeleteObject(cq, absl::make_unique<grpc::ClientContext>(),
                               request)
            .get();
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, AsyncReadObject) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncReadObject).WillOnce(MakeAsyncReadObjectStream);
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  google::cloud::CompletionQueue cq;
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::ReadObjectRequest request;
    auto response = under_test.AsyncReadObject(
        cq, absl::make_unique<grpc::ClientContext>(), request);
    EXPECT_FALSE(response->Read().get());
    auto status = response->Finish().get();
    EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, AsyncWriteObject) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncWriteObject).WillOnce(MakeAsyncWriteObjectStream);
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  google::cloud::CompletionQueue cq;
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto stream = under_test.AsyncWriteObject(
        cq, absl::make_unique<grpc::ClientContext>());
    EXPECT_FALSE(stream->WritesDone().get());
    auto response = stream->Finish().get();
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, AsyncStartResumableWrite) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncStartResumableWrite).WillOnce([](auto, auto, auto) {
        auto response =
            StatusOr<google::storage::v2::StartResumableWriteResponse>(
                Status(StatusCode::kPermissionDenied, "uh-oh"));
        return make_ready_future(response);
      });
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  google::cloud::CompletionQueue cq;
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::StartResumableWriteRequest request;
    auto response =
        under_test
            .AsyncStartResumableWrite(
                cq, absl::make_unique<grpc::ClientContext>(), request)
            .get();
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageRoundRobinTest, AsyncQueryWriteStatus) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncQueryWriteStatus).WillOnce([](auto, auto, auto) {
        auto response = StatusOr<google::storage::v2::QueryWriteStatusResponse>(
            Status(StatusCode::kPermissionDenied, "uh-oh"));
        return make_ready_future(response);
      });
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  google::cloud::CompletionQueue cq;
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v2::QueryWriteStatusRequest request;
    auto response =
        under_test
            .AsyncQueryWriteStatus(cq, absl::make_unique<grpc::ClientContext>(),
                                   request)
            .get();
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
