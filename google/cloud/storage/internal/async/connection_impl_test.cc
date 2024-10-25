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

#include "google/cloud/storage/internal/async/connection_impl.h"
#include "google/cloud/storage/async/idempotency_policy.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/internal/async/write_payload_impl.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "google/protobuf/text_format.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::protobuf::TextFormat;
using ::testing::AllOf;
using ::testing::ResultOf;

class AsyncConnectionImplTest : public ::testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

auto constexpr kAuthority = "storage.googleapis.com";

auto TestOptions(Options options = {}) {
  using ms = std::chrono::milliseconds;
  options = internal::MergeOptions(
      std::move(options),
      Options{}
          .set<GrpcNumChannelsOption>(1)
          .set<storage::RetryPolicyOption>(
              storage::LimitedErrorCountRetryPolicy(2).clone())
          .set<storage::BackoffPolicyOption>(
              storage::ExponentialBackoffPolicy(ms(1), ms(2), 2.0).clone()));
  return DefaultOptionsAsync(std::move(options));
}

std::shared_ptr<storage_experimental::AsyncConnection> MakeTestConnection(
    CompletionQueue cq, std::shared_ptr<storage::testing::MockStorageStub> mock,
    Options options = {}) {
  return MakeAsyncConnection(std::move(cq), std::move(mock),
                             TestOptions(std::move(options)));
}

TEST_F(AsyncConnectionImplTest, ComposeObject) {
  auto constexpr kExpectedRequest = R"pb(
    destination { bucket: "projects/_/buckets/test-bucket" name: "test-object" }
    source_objects { name: "input-0" }
    source_objects { name: "input-1" }
    if_generation_match: 0
  )pb";
  auto constexpr kExpectedObject = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    name: "test-object"
    size: 4096
    component_count: 2
  )pb";

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncComposeObject)
      .WillOnce([&] {
        return sequencer.PushBack("ComposeObject(1)").then([](auto) {
          return StatusOr<google::storage::v2::Object>(TransientError());
        });
      })
      .WillOnce([&](CompletionQueue&, auto,
                    google::cloud::internal::ImmutableOptions const& options,
                    google::storage::v2::ComposeObjectRequest const& request) {
        // Verify at least one option is initialized with the correct value.
        EXPECT_EQ(options->get<AuthorityOption>(), kAuthority);
        auto expected = google::storage::v2::ComposeObjectRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("ComposeObject(2)").then([&](auto) {
          auto result = google::storage::v2::Object{};
          EXPECT_TRUE(TextFormat::ParseFromString(kExpectedObject, &result));
          return make_status_or(std::move(result));
        });
      });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto request = google::storage::v2::ComposeObjectRequest{};
  EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto pending =
      connection->ComposeObject({std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "ComposeObject(1)");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "ComposeObject(2)");
  next.first.set_value(true);

  auto expected = google::storage::v2::Object{};
  EXPECT_TRUE(TextFormat::ParseFromString(kExpectedObject, &expected));
  EXPECT_THAT(pending.get(), IsOkAndHolds(IsProtoEqual(expected)));
}

TEST_F(AsyncConnectionImplTest, ComposeObjectPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncComposeObject).WillOnce([&] {
    return sequencer.PushBack("ComposeObject").then([](auto) {
      return StatusOr<google::storage::v2::Object>(PermanentError());
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->ComposeObject(
      {google::storage::v2::ComposeObjectRequest{}, connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "ComposeObject");
  next.first.set_value(false);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(PermanentError().code()));
}

TEST_F(AsyncConnectionImplTest, ComposeObjectTooManyTransients) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncComposeObject).Times(3).WillRepeatedly([&] {
    return sequencer.PushBack("ComposeObject").then([](auto) {
      return StatusOr<google::storage::v2::Object>(TransientError());
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  // Use a policy that makes a default-initialized request retryable.
  auto connection = MakeTestConnection(
      pool.cq(), mock,
      Options{}.set<storage_experimental::IdempotencyPolicyOption>(
          storage_experimental::MakeAlwaysRetryIdempotencyPolicy));
  auto pending = connection->ComposeObject(
      {google::storage::v2::ComposeObjectRequest{}, connection->options()});

  for (int i = 0; i != 3; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "ComposeObject");
    next.first.set_value(false);
  }

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(TransientError().code()));
}

TEST_F(AsyncConnectionImplTest, DeleteObject) {
  auto constexpr kRequestText = R"pb(
    bucket: "invalid-test-only"
    object: "test-object"
    generation: 12345
    if_metageneration_match: 42
  )pb";
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncDeleteObject)
      .WillOnce([&] {
        return sequencer.PushBack("DeleteObject(1)").then([](auto) {
          return TransientError();
        });
      })
      .WillOnce([&](CompletionQueue&, auto,
                    google::cloud::internal::ImmutableOptions const& options,
                    google::storage::v2::DeleteObjectRequest const& request) {
        // Verify at least one option is initialized with the correct values.
        EXPECT_EQ(options->get<AuthorityOption>(), kAuthority);
        google::storage::v2::DeleteObjectRequest expected;
        EXPECT_TRUE(TextFormat::ParseFromString(kRequestText, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("DeleteObject(2)").then([](auto) {
          return Status{};
        });
      });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  google::storage::v2::DeleteObjectRequest request;
  EXPECT_TRUE(TextFormat::ParseFromString(kRequestText, &request));
  auto pending =
      connection->DeleteObject({std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "DeleteObject(1)");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "DeleteObject(2)");
  next.first.set_value(true);

  auto response = pending.get();
  ASSERT_STATUS_OK(response);
}

TEST_F(AsyncConnectionImplTest, DeleteObjectPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncDeleteObject).WillOnce([&] {
    return sequencer.PushBack("DeleteObject").then([](auto) {
      return PermanentError();
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->DeleteObject(
      {google::storage::v2::DeleteObjectRequest{}, connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "DeleteObject");
  next.first.set_value(false);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(PermanentError().code()));
}

TEST_F(AsyncConnectionImplTest, AsyncDeleteObjectTooManyTransients) {
  auto constexpr kRequestText =
      R"pb(
    bucket: "invalid-test-only" object: "test-object" generation: 12345
      )pb";

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncDeleteObject).Times(3).WillRepeatedly([&] {
    return sequencer.PushBack("DeleteObject").then([](auto) {
      return TransientError();
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  google::storage::v2::DeleteObjectRequest request;
  EXPECT_TRUE(TextFormat::ParseFromString(kRequestText, &request));
  auto pending =
      connection->DeleteObject({std::move(request), connection->options()});

  for (int i = 0; i != 3; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "DeleteObject");
    next.first.set_value(false);
  }

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(TransientError().code()));
}

// For RewriteObject just validate the basic functionality. The tests for
// `RewriterConnectionImpl` are the important ones.
TEST_F(AsyncConnectionImplTest, RewriteObject) {
  using ::google::storage::v2::RewriteObjectRequest;
  using ::google::storage::v2::RewriteResponse;

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncRewriteObject)
      .WillOnce([&] {
        return sequencer.PushBack("RewriteObject(1)").then([](auto) {
          return StatusOr<google::storage::v2::RewriteResponse>(
              TransientError());
        });
      })
      .WillOnce([&] {
        return sequencer.PushBack("RewriteObject(2)").then([](auto) {
          google::storage::v2::RewriteResponse response;
          response.set_total_bytes_rewritten(1000);
          response.set_object_size(3000);
          response.set_rewrite_token("test-rewrite-token");
          return make_status_or(response);
        });
      });

  auto match_progress = [](int rewritten, int size) {
    return AllOf(
        ResultOf(
            "total bytes",
            [](RewriteResponse const& v) { return v.total_bytes_rewritten(); },
            rewritten),
        ResultOf(
            "size", [](RewriteResponse const& v) { return v.object_size(); },
            size),
        ResultOf(
            "token", [](RewriteResponse const& v) { return v.rewrite_token(); },
            "test-rewrite-token"));
  };

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto rewriter = connection->RewriteObject(
      {RewriteObjectRequest{}, connection->options()});

  auto r1 = rewriter->Iterate();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "RewriteObject(1)");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "RewriteObject(2)");
  next.first.set_value(true);
  EXPECT_THAT(r1.get(), IsOkAndHolds(match_progress(1000, 3000)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
