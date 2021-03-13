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

#include "google/cloud/internal/resumable_streaming_read_rpc.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Return;

struct FakeRequest {
  std::string key;
  std::string token;
};

struct FakeResponse {
  std::string value;
  std::string token;
};

using MockReturnType =
    std::unique_ptr<grpc::ClientReaderInterface<FakeResponse>>;

using ReadReturn = absl::variant<Status, FakeResponse>;

class MockStreamingReadRpc : public StreamingReadRpc<FakeResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(ReadReturn, Read, (), (override));
};

struct TestRetryablePolicy {
  static bool IsPermanentFailure(google::cloud::Status const& s) {
    return !s.ok() &&
           (s.code() == google::cloud::StatusCode::kPermissionDenied);
  }
};

ReadReturn AsReadReturn(FakeResponse response) { return response; }

ReadReturn StreamSuccess() { return Status{}; }

ReadReturn TransientFailure() {
  return Status(StatusCode::kUnavailable, "try-again");
}

ReadReturn PermanentFailure() {
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}

class MockStub {
 public:
  MOCK_METHOD(std::unique_ptr<StreamingReadRpc<FakeResponse>>, StreamingRead,
              (FakeRequest const&), ());
};

using RetryPolicyForTest =
    google::cloud::internal::TraitBasedRetryPolicy<TestRetryablePolicy>;
using LimitedTimeRetryPolicyForTest =
    google::cloud::internal::LimitedTimeRetryPolicy<TestRetryablePolicy>;
using LimitedErrorCountRetryPolicyForTest =
    google::cloud::internal::LimitedErrorCountRetryPolicy<TestRetryablePolicy>;

std::unique_ptr<RetryPolicyForTest> DefaultRetryPolicy() {
  // With maximum_failures==2 it tolerates up to 2 failures, so the *third*
  // failure is an error.
  return LimitedErrorCountRetryPolicyForTest(/*maximum_failures=*/2).clone();
}

std::unique_ptr<BackoffPolicy> DefaultBackoffPolicy() {
  using us = std::chrono::microseconds;
  return ExponentialBackoffPolicy(/*initial_delay=*/us(10),
                                  /*maximum_delay=*/us(40), /*scaling=*/2.0)
      .clone();
}

void DefaultUpdater(FakeResponse const& response, FakeRequest& request) {
  request.token = response.token;
}

TEST(ResumableStreamingReadRpc, ResumeWithPartials) {
  MockStub mock;
  EXPECT_CALL(mock, StreamingRead)
      .WillOnce([](FakeRequest const& request) {
        EXPECT_EQ(request.key, "test-key");
        EXPECT_THAT(request.token, IsEmpty());
        auto stream = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(AsReadReturn(FakeResponse{"value-0", "token-1"})))
            .WillOnce(Return(AsReadReturn(FakeResponse{"value-1", "token-2"})))
            .WillOnce(Return(TransientFailure()));
        return stream;
      })
      .WillOnce([](FakeRequest const& request) {
        EXPECT_EQ(request.key, "test-key");
        EXPECT_THAT(request.token, "token-2");
        auto stream = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(AsReadReturn(FakeResponse{"value-2", "token-2"})))
            .WillOnce(Return(StreamSuccess()));
        return stream;
      });
  auto reader = MakeResumableStreamingReadRpc<FakeResponse, FakeRequest>(
      DefaultRetryPolicy(), DefaultBackoffPolicy(),
      [](std::chrono::milliseconds) {},
      [&mock](FakeRequest const& request) {
        return mock.StreamingRead(request);
      },
      DefaultUpdater, FakeRequest{"test-key", {}});

  std::vector<std::string> values;
  for (;;) {
    auto v = reader->Read();
    if (absl::holds_alternative<FakeResponse>(v)) {
      values.push_back(absl::get<FakeResponse>(std::move(v)).value);
      continue;
    }
    EXPECT_THAT(absl::get<Status>(std::move(v)), IsOk());
    break;
  }
  EXPECT_THAT(values, ElementsAre("value-0", "value-1", "value-2"));
}

TEST(ResumableStreamingReadRpc, TooManyTransientFailures) {
  MockStub mock;
  EXPECT_CALL(mock, StreamingRead)
      .WillOnce([](FakeRequest const&) {
        auto stream = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(TransientFailure()));
        return stream;
      })
      .WillOnce([](FakeRequest const&) {
        auto stream = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(TransientFailure()));
        return stream;
      })
      .WillOnce([](FakeRequest const&) {
        // Event though this stream ends with a failure it will be resumed
        // because its successful Read() resets the retry policy.
        auto stream = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(AsReadReturn(FakeResponse{"value-0", "token-1"})))
            .WillOnce(Return(TransientFailure()));
        return stream;
      })
      .WillOnce([](FakeRequest const&) {
        auto stream = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(TransientFailure()));
        return stream;
      })
      .WillOnce([](FakeRequest const&) {
        auto stream = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(TransientFailure()));
        return stream;
      })
      .WillOnce([](FakeRequest const&) {
        auto stream = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(TransientFailure()));
        return stream;
      });
  auto reader = MakeResumableStreamingReadRpc<FakeResponse, FakeRequest>(
      DefaultRetryPolicy(), DefaultBackoffPolicy(),
      [](std::chrono::milliseconds) {},
      [&mock](FakeRequest const& request) {
        return mock.StreamingRead(request);
      },
      DefaultUpdater, FakeRequest{"test-key", {}});

  std::vector<std::string> values;
  for (;;) {
    auto v = reader->Read();
    if (absl::holds_alternative<FakeResponse>(v)) {
      values.push_back(absl::get<FakeResponse>(std::move(v)).value);
      continue;
    }
    EXPECT_THAT(absl::get<Status>(std::move(v)),
                StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
    break;
  }
  EXPECT_THAT(values, ElementsAre("value-0"));
}

TEST(ResumableStreamingReadRpc, PermanentFailure) {
  MockStub mock;
  EXPECT_CALL(mock, StreamingRead)
      .WillOnce([](FakeRequest const&) {
        // Event though this stream ends with a failure it will be resumed
        // because its successful Read() resets the retry policy.
        auto stream = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(AsReadReturn(FakeResponse{"value-0", "token-1"})))
            .WillOnce(Return(PermanentFailure()));
        return stream;
      })
      .WillOnce([](FakeRequest const&) {
        auto stream = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(PermanentFailure()));
        return stream;
      });
  auto reader = MakeResumableStreamingReadRpc<FakeResponse, FakeRequest>(
      DefaultRetryPolicy(), DefaultBackoffPolicy(),
      [](std::chrono::milliseconds) {},
      [&mock](FakeRequest const& request) {
        return mock.StreamingRead(request);
      },
      DefaultUpdater, FakeRequest{"test-key", {}});

  std::vector<std::string> values;
  for (;;) {
    auto v = reader->Read();
    if (absl::holds_alternative<FakeResponse>(v)) {
      values.push_back(absl::get<FakeResponse>(std::move(v)).value);
      continue;
    }
    EXPECT_THAT(absl::get<Status>(std::move(v)),
                StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
    break;
  }
  EXPECT_THAT(values, ElementsAre("value-0"));
}

TEST(ResumableStreamingReadRpc, PermanentFailureAtStart) {
  MockStub mock;
  EXPECT_CALL(mock, StreamingRead).WillOnce([](FakeRequest const&) {
    auto stream = absl::make_unique<MockStreamingReadRpc>();
    EXPECT_CALL(*stream, Read).WillOnce(Return(PermanentFailure()));
    return stream;
  });
  auto reader = MakeResumableStreamingReadRpc<FakeResponse, FakeRequest>(
      DefaultRetryPolicy(), DefaultBackoffPolicy(),
      [](std::chrono::milliseconds) {},
      [&mock](FakeRequest const& request) {
        return mock.StreamingRead(request);
      },
      DefaultUpdater, FakeRequest{"test-key", {}});

  std::vector<std::string> values;
  for (;;) {
    auto v = reader->Read();
    if (absl::holds_alternative<FakeResponse>(v)) {
      values.push_back(absl::get<FakeResponse>(std::move(v)).value);
      continue;
    }
    EXPECT_THAT(absl::get<Status>(std::move(v)),
                StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
    break;
  }
  EXPECT_THAT(values, IsEmpty());
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
