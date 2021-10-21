// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/storage_round_robin.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
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

std::unique_ptr<StorageStub::ObjectMediaStream> MakeObjectMediaStream(
    std::unique_ptr<grpc::ClientContext>,
    google::storage::v1::GetObjectMediaRequest const&) {
  using ErrorStream = google::cloud::internal::StreamingReadRpcError<
      google::storage::v1::GetObjectMediaResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

std::unique_ptr<StorageStub::InsertStream> MakeInsertStream(
    std::unique_ptr<grpc::ClientContext>) {
  using ErrorStream = google::cloud::internal::StreamingWriteRpcError<
      google::storage::v1::InsertObjectRequest, google::storage::v1::Object>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

TEST(StorageAuthTest, GetObjectMedia) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetObjectMedia).WillOnce(MakeObjectMediaStream);
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::GetObjectMediaRequest request;
    auto response = under_test.GetObjectMedia(
        absl::make_unique<grpc::ClientContext>(), request);
    auto v = response->Read();
    ASSERT_TRUE(absl::holds_alternative<Status>(v));
    EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, InsertObjectMedia) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, InsertObjectMedia).WillOnce(MakeInsertStream);
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    auto response =
        under_test.InsertObjectMedia(absl::make_unique<grpc::ClientContext>());
    EXPECT_THAT(response->Close(), StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, DeleteBucketAccessControl) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DeleteBucketAccessControl)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::DeleteBucketAccessControlRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.DeleteBucketAccessControl(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, GetBucketAccessControl) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetBucketAccessControl)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::GetBucketAccessControlRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.GetBucketAccessControl(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, InsertBucketAccessControl) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, InsertBucketAccessControl)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::InsertBucketAccessControlRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.InsertBucketAccessControl(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, ListBucketAccessControls) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListBucketAccessControls)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::ListBucketAccessControlsRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.ListBucketAccessControls(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, UpdateBucketAccessControl) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, UpdateBucketAccessControl)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::UpdateBucketAccessControlRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.UpdateBucketAccessControl(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, DeleteBucket) {
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
    google::storage::v1::DeleteBucketRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.DeleteBucket(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, GetBucket) {
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
    google::storage::v1::GetBucketRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.GetBucket(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, InsertBucket) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, InsertBucket)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::InsertBucketRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.InsertBucket(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, ListBuckets) {
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
    google::storage::v1::ListBucketsRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.ListBuckets(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, GetBucketIamPolicy) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetBucketIamPolicy)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::GetIamPolicyRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.GetBucketIamPolicy(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, SetBucketIamPolicy) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, SetBucketIamPolicy)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::SetIamPolicyRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.SetBucketIamPolicy(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, TestBucketIamPermissions) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, TestBucketIamPermissions)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::TestIamPermissionsRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.TestBucketIamPermissions(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, UpdateBucket) {
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
    google::storage::v1::UpdateBucketRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.UpdateBucket(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, DeleteDefaultObjectAccessControl) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DeleteDefaultObjectAccessControl)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::DeleteDefaultObjectAccessControlRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.DeleteDefaultObjectAccessControl(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, GetDefaultObjectAccessControl) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetDefaultObjectAccessControl)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::GetDefaultObjectAccessControlRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.GetDefaultObjectAccessControl(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, InsertDefaultObjectAccessControl) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, InsertDefaultObjectAccessControl)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::InsertDefaultObjectAccessControlRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.InsertDefaultObjectAccessControl(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, ListDefaultObjectAccessControls) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListDefaultObjectAccessControls)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::ListDefaultObjectAccessControlsRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.ListDefaultObjectAccessControls(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, UpdateDefaultObjectAccessControl) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, UpdateDefaultObjectAccessControl)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::UpdateDefaultObjectAccessControlRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.UpdateDefaultObjectAccessControl(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, DeleteNotification) {
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
    google::storage::v1::DeleteNotificationRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.DeleteNotification(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, GetNotification) {
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
    google::storage::v1::GetNotificationRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.GetNotification(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, InsertNotification) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, InsertNotification)
          .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
    }
  }

  StorageRoundRobin under_test(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::storage::v1::InsertNotificationRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.InsertNotification(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, ListNotifications) {
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
    google::storage::v1::ListNotificationsRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.ListNotifications(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, DeleteObject) {
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
    google::storage::v1::DeleteObjectRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.DeleteObject(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, StartResumableWrite) {
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
    google::storage::v1::StartResumableWriteRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.StartResumableWrite(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

TEST(StorageAuthTest, QueryWriteStatus) {
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
    google::storage::v1::QueryWriteStatusRequest request;
    grpc::ClientContext ctx;
    auto response = under_test.QueryWriteStatus(ctx, request);
    EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
  }
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
