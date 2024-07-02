// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/retry_object_read_source.h"
#include "google/cloud/storage/internal/connection_impl.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/mock_generic_stub.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockGenericStub;
using ::google::cloud::storage::testing::MockObjectReadSource;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::Return;

class BackoffPolicyMockState {
 public:
  MOCK_METHOD(std::chrono::milliseconds, OnCompletion, ());

  int num_calls_from_last_clone_{0};
  int num_clones_{0};
};

// Pretend independent backoff policies, but be only one under the hood.
// This is a trick to count the number of `clone()` calls.
class BackoffPolicyMock : public BackoffPolicy {
 public:
  BackoffPolicyMock() : state_(new BackoffPolicyMockState) {}

  std::chrono::milliseconds OnCompletion() override {
    ++state_->num_calls_from_last_clone_;
    return state_->OnCompletion();
  }

  std::unique_ptr<BackoffPolicy> clone() const override {
    state_->num_calls_from_last_clone_ = 0;
    ++state_->num_clones_;
    return std::unique_ptr<BackoffPolicy>(new BackoffPolicyMock(*this));
  }

  int NumCallsFromLastClone() { return state_->num_calls_from_last_clone_; }
  int NumClones() { return state_->num_clones_; }

  std::shared_ptr<BackoffPolicyMockState> state_;
};

Options BasicTestPolicies() {
  using us = std::chrono::microseconds;
  return Options{}
      .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(3).clone())
      .set<BackoffPolicyOption>(
          // Make the tests faster.
          ExponentialBackoffPolicy(us(1), us(2), 2).clone())
      .set<IdempotencyPolicyOption>(StrictIdempotencyPolicy().clone());
}

/// @test No failures scenario.
TEST(RetryObjectReadSourceTest, NoFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, ReadObject).WillOnce([] {
    auto source = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{}));
    return std::unique_ptr<ObjectReadSource>(std::move(source));
  });

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  auto source = client->ReadObject(ReadObjectRangeRequest{});
  ASSERT_STATUS_OK(source);
  ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
}

/// @test Permanent failure when creating the raw source
TEST(RetryObjectReadSourceTest, PermanentFailureOnSessionCreation) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, ReadObject).WillOnce(Return(PermanentError()));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  auto source = client->ReadObject(ReadObjectRangeRequest{});
  ASSERT_FALSE(source);
  EXPECT_EQ(PermanentError().code(), source.status().code());
}

/// @test Transient failures exhaust retry policy when creating the raw source
TEST(RetryObjectReadSourceTest, TransientFailuresExhaustOnSessionCreation) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, ReadObject).Times(4).WillRepeatedly([] {
    return TransientError();
  });

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  auto source = client->ReadObject(ReadObjectRangeRequest{});
  ASSERT_FALSE(source);
  EXPECT_EQ(TransientError().code(), source.status().code());
}

/// @test Recovery from a transient failures when creating the raw source
TEST(RetryObjectReadSourceTest, SessionCreationRecoversFromTransientFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce(Return(TransientError()))
      .WillOnce(Return(TransientError()))
      .WillOnce([](auto const&, auto const&, ReadObjectRangeRequest const&) {
        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{}));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      });

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  auto source = client->ReadObject(ReadObjectRangeRequest{});
  ASSERT_STATUS_OK(source);
  EXPECT_STATUS_OK((*source)->Read(nullptr, 1024));
}

/// @test A permanent error after a successful read
TEST(RetryObjectReadSourceTest, PermanentReadFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, ReadObject).WillOnce([] {
    auto source = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*source, Read)
        .WillOnce(Return(ReadSourceResult{}))
        .WillOnce(Return(PermanentError()));
    return std::unique_ptr<ObjectReadSource>(std::move(source));
  });

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  auto source = client->ReadObject(ReadObjectRangeRequest{});
  ASSERT_STATUS_OK(source);
  ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
  auto res = (*source)->Read(nullptr, 1024);
  ASSERT_FALSE(res);
  EXPECT_EQ(PermanentError().code(), res.status().code());
}

/// @test Test if backoff policy is reset on success
TEST(RetryObjectReadSourceTest, BackoffPolicyResetOnSuccess) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([] {
        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read).WillOnce(Return(TransientError()));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      })
      .WillOnce([] {
        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read).WillOnce(Return(TransientError()));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      })
      .WillOnce([] {
        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read)
            .WillOnce(Return(ReadSourceResult{}))
            .WillOnce(Return(TransientError()));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      })
      .WillOnce([] {
        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{}));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      });

  int num_backoff_policy_called = 0;
  BackoffPolicyMock backoff_policy_mock;
  EXPECT_CALL(*backoff_policy_mock.state_, OnCompletion()).WillRepeatedly([&] {
    ++num_backoff_policy_called;
    return std::chrono::milliseconds(0);
  });
  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<BackoffPolicyOption>(
          backoff_policy_mock.clone()));

  EXPECT_EQ(0, num_backoff_policy_called);

  // We really do not care how many times the policy is closed before we make
  // the first `Read()` call.  We only care about how it increases.
  auto const initial_clone_count = backoff_policy_mock.NumClones();
  auto source = client->ReadObject(ReadObjectRangeRequest{});
  ASSERT_STATUS_OK(source);
  EXPECT_EQ(initial_clone_count + 1, backoff_policy_mock.NumClones());
  EXPECT_EQ(0, num_backoff_policy_called);

  // raw_source1 and raw_source2 fail, then a success
  ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
  // Two retries, the first one does not get a backoff, so there should be one
  // call to OnCompletion.
  EXPECT_EQ(1, num_backoff_policy_called);
  // The backoff should have been cloned during the Read() call.
  EXPECT_EQ(initial_clone_count + 2, backoff_policy_mock.NumClones());
  // The backoff policy was used once in the first retry.
  EXPECT_EQ(1, backoff_policy_mock.NumCallsFromLastClone());

  // raw_source3 fails, then a success
  ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
  // This read caused another retry, but no call to backoff.
  EXPECT_EQ(1, num_backoff_policy_called);
  // The backoff should have been cloned during the read.
  EXPECT_EQ(initial_clone_count + 3, backoff_policy_mock.NumClones());
  // The backoff policy was cloned once, but never used in the second retry.
  EXPECT_EQ(0, backoff_policy_mock.NumCallsFromLastClone());
}

/// @test Check that retry policy is shared between reads and resetting session
TEST(RetryObjectReadSourceTest, RetryPolicyExhaustedOnResetSession) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([] {
        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read)
            .WillOnce(Return(ReadSourceResult{}))
            .WillOnce(Return(TransientError()));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      })
      .WillOnce([] { return TransientError(); })
      .WillOnce([] { return TransientError(); })
      .WillOnce([] { return TransientError(); });

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  auto source = client->ReadObject(ReadObjectRangeRequest{});
  ASSERT_STATUS_OK(source);
  ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
  auto res = (*source)->Read(nullptr, 1024);
  // It takes 4 retry attempts to exhaust the policy. Only a retry policy shared
  // between reads and resetting the session could exhaust it.
  ASSERT_FALSE(res);
  EXPECT_EQ(TransientError().code(), res.status().code());
  EXPECT_THAT(res.status().message(), HasSubstr("Retry policy exhausted"));
}

/// @test Check that retry policy is shared between reads and resetting session
TEST(RetryObjectReadSourceTest, ResumePolicyOrder) {
  ::testing::MockFunction<void(std::chrono::milliseconds)> backoff;
  ::testing::MockFunction<StatusOr<std::unique_ptr<ObjectReadSource>>(
      ReadObjectRangeRequest const&, RetryPolicy&, BackoffPolicy&)>
      factory;

  auto make_partial = [] {
    auto source = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*source, Read).WillOnce(Return(TransientError()));
    return std::unique_ptr<ObjectReadSource>(std::move(source));
  };

  auto source = std::make_unique<MockObjectReadSource>();
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Read)
        .WillOnce(Return(ReadSourceResult{static_cast<std::size_t>(512 * 1024),
                                          HttpResponse{100, "", {}}}));
    EXPECT_CALL(*source, Read).WillOnce(Return(TransientError()));
    // No backoffs to resume after a (partially) successful request:
    //    EXPECT_CALL(backoff, Call).Times(1);
    EXPECT_CALL(factory, Call).WillOnce(make_partial);
    EXPECT_CALL(backoff, Call).Times(1);
    EXPECT_CALL(factory, Call).WillOnce(make_partial);
    EXPECT_CALL(backoff, Call).Times(1);
    EXPECT_CALL(factory, Call).WillOnce(make_partial);
  }

  auto options = BasicTestPolicies();
  auto retry_policy = options.get<RetryPolicyOption>()->clone();
  auto backoff_policy = options.get<BackoffPolicyOption>()->clone();

  RetryObjectReadSource tested(
      factory.AsStdFunction(),
      google::cloud::internal::MakeImmutableOptions(std::move(options)),
      ReadObjectRangeRequest{}, std::move(source), std::move(retry_policy),
      std::move(backoff_policy), backoff.AsStdFunction());

  ASSERT_STATUS_OK(tested.Read(nullptr, 1024));
  auto response = tested.Read(nullptr, 1024);
  EXPECT_THAT(response, StatusIs(TransientError().code()));
}

/// @test `ReadLast` behaviour after a transient failure
TEST(RetryObjectReadSourceTest, TransientFailureWithReadLastOption) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([](auto&, auto const&, ReadObjectRangeRequest const& req) {
        EXPECT_EQ(1029, req.GetOption<ReadLast>().value());
        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read)
            .WillOnce(Return(ReadSourceResult{static_cast<std::size_t>(1024),
                                              HttpResponse{100, "", {}}}))
            .WillOnce(Return(TransientError()));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      })
      .WillOnce([](auto&, auto const&, ReadObjectRangeRequest const& req) {
        EXPECT_EQ(5, req.GetOption<ReadLast>().value());
        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{}));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      });

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  ReadObjectRangeRequest req("test_bucket", "test_object");
  req.set_option(ReadLast(1029));
  auto source = client->ReadObject(req);
  ASSERT_STATUS_OK(source);
  ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
  auto res = (*source)->Read(nullptr, 1024);
  ASSERT_TRUE(res);
}

/// @test The generation is captured such that we resume from the same version
TEST(RetryObjectReadSourceTest, TransientFailureWithGeneration) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([](auto&, auto const&, ReadObjectRangeRequest const& req) {
        EXPECT_FALSE(req.HasOption<ReadRange>());
        EXPECT_FALSE(req.HasOption<Generation>());
        auto source = std::make_unique<MockObjectReadSource>();
        auto result = ReadSourceResult{static_cast<std::size_t>(1024),
                                       HttpResponse{200, "", {}}};
        result.generation = 23456;
        EXPECT_CALL(*source, Read)
            .WillOnce(Return(result))
            .WillOnce(Return(TransientError()));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      })
      .WillOnce([](auto&, auto const&, ReadObjectRangeRequest const& req) {
        EXPECT_EQ(1024, req.GetOption<ReadFromOffset>().value_or(0));
        EXPECT_EQ(23456, req.GetOption<Generation>().value_or(0));
        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{}));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      });

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  ReadObjectRangeRequest req("test_bucket", "test_object");
  auto source = client->ReadObject(req);
  ASSERT_STATUS_OK(source);
  ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
  auto res = (*source)->Read(nullptr, 1024);
  ASSERT_TRUE(res);
}

/// @test Downloads with decompressive transcoding require discarding data.
TEST(RetryObjectReadSourceTest, DiscardDataForDecompressiveTranscoding) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([] {
        // Simulate an initial download that reveals the object is subject to
        // decompressive transcoding. It returns the requested amount of data
        // (512 * 1024 bytes), and then fails with a transient error.
        auto source = std::make_unique<MockObjectReadSource>();
        auto r0 = ReadSourceResult{
            static_cast<std::size_t>(512 * 1024),
            HttpResponse{100, "", {{"x-test-only", "download 1 r0"}}}};
        r0.generation = 23456;
        r0.transformation = "gunzipped";
        EXPECT_CALL(*source, Read)
            .WillOnce(Return(r0))
            .WillOnce(Return(TransientError()));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      })
      .WillOnce([](auto&, auto const&, ReadObjectRangeRequest const& req) {
        // The previous transient error should trigger a second download. We
        // simulate a successful start, but the download is interrupted before
        // it can reach the previous offset.
        EXPECT_EQ(512 * 1024, req.GetOption<ReadFromOffset>().value_or(0));
        EXPECT_EQ(23456, req.GetOption<Generation>().value_or(0));
        auto r0 = ReadSourceResult{
            static_cast<std::size_t>(128 * 1024),
            HttpResponse{100, "", {{"x-test-only", "download 2 r0"}}}};
        r0.generation = 23456;
        r0.transformation = "gunzipped";

        auto r1 = ReadSourceResult{
            static_cast<std::size_t>(128 * 1024),
            HttpResponse{200, "", {{"x-test-only", "download 2 r1"}}}};

        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read).WillOnce(Return(r0)).WillOnce(Return(r1));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      })
      .WillOnce([] {
        // Because the previous download "completes" before reaching the desired
        // offset, we need to start a third download. Let's have this one fail
        // immediately.
        return TransientError();
      })
      .WillOnce([](auto&, auto const&, ReadObjectRangeRequest const& req) {
        // That triggers a fourth download. We simulate a successful download.
        EXPECT_EQ(512 * 1024, req.GetOption<ReadFromOffset>().value_or(0));
        EXPECT_EQ(23456, req.GetOption<Generation>().value_or(0));
        // We expect the RetryReadObjectSource class to initiative a third
        // download. Let's make it succeed, but then return a short download
        // on the next attempt.
        auto r0 = ReadSourceResult{
            static_cast<std::size_t>(128 * 1024),
            HttpResponse{100, "", {{"x-test-only", "download 3 r0"}}}};
        r0.generation = 23456;
        r0.transformation = "gunzipped";

        auto r1 = ReadSourceResult{
            static_cast<std::size_t>(64 * 1024),
            HttpResponse{200, "", {{"x-test-only", "download 3 r1"}}}};
        r1.generation = 23456;
        r1.transformation = "gunzipped";

        auto source = std::make_unique<MockObjectReadSource>();
        // We expect 4 reads to reach the desired offset, and then return the
        // real data.
        ::testing::InSequence sequence;
        EXPECT_CALL(*source, Read(_, 128 * 1024L))
            .Times(4)
            .WillRepeatedly(Return(r0));
        EXPECT_CALL(*source, Read(_, 256 * 1024L)).WillOnce(Return(r1));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
      });

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<RetryPolicyOption>(
          LimitedErrorCountRetryPolicy(10).clone()));

  std::vector<char> buffer(1024 * 1024L);

  ReadObjectRangeRequest req("test_bucket", "test_object");
  auto source = client->ReadObject(req);
  ASSERT_STATUS_OK(source);
  auto response = (*source)->Read(nullptr, 512 * 1024);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bytes_received, 512 * 1024);
  EXPECT_EQ(response->transformation.value_or(""), "gunzipped");
  EXPECT_THAT(response->response.headers,
              Contains(Pair("x-test-only", "download 1 r0")));

  response = (*source)->Read(nullptr, 256 * 1024);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bytes_received, 64 * 1024);
  EXPECT_EQ(response->transformation.value_or(""), "gunzipped");
  EXPECT_THAT(response->response.headers,
              Contains(Pair("x-test-only", "download 3 r1")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
