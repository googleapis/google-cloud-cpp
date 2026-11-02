// Copyright 2024 Google LLC
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

#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/async/retry_policy.h"
#include "google/cloud/storage/internal/async/connection_impl.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/internal/async/options.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_resume_policy.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/mock_opentelemetry_metrics.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/match.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <opentelemetry/metrics/provider.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::InvokeWithoutArgs;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Optional;

using BidiReadStream = google::cloud::AsyncStreamingReadWriteRpc<
    google::storage::v2::BidiReadObjectRequest,
    google::storage::v2::BidiReadObjectResponse>;
using MockStream = google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
    google::storage::v2::BidiReadObjectRequest,
    google::storage::v2::BidiReadObjectResponse>;

auto constexpr kAuthority = "storage.googleapis.com";
auto constexpr kRetryAttempts = 2;

// The `[gcs-retry]` records, in order. Everything else the library logs is
// noise for these tests.
std::vector<std::string> RetryRecords(ScopedLog& log) {
  std::vector<std::string> records;
  for (auto const& line : log.ExtractLines()) {
    if (absl::StrContains(line, "[gcs-retry]")) records.push_back(line);
  }
  return records;
}

auto TestOptions(Options options = {}) {
  using ms = std::chrono::milliseconds;
  options = internal::MergeOptions(
      std::move(options),
      Options{}
          .set<GrpcNumChannelsOption>(1)
          // By default, disable timeouts, most tests are simpler without them.
          .set<storage::DownloadStallTimeoutOption>(std::chrono::seconds(0))
          // By default, disable resumes, most tests are simpler without them.
          .set<storage::ResumePolicyOption>(
              storage::LimitedErrorCountResumePolicy(0))
          .set<storage::AsyncRetryPolicyOption>(
              storage::LimitedErrorCountAsyncRetryPolicy(kRetryAttempts)
                  .clone())
          .set<storage::BackoffPolicyOption>(
              storage::ExponentialBackoffPolicy(ms(1), ms(2), 2.0).clone()));
  return DefaultOptionsAsync(std::move(options));
}

std::unique_ptr<BidiReadStream> MakeErrorStream(AsyncSequencer<void>& sequencer,
                                                Status const& status) {
  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Start).WillOnce([&] {
    return sequencer.PushBack("Start").then([](auto) { return false; });
  });
  EXPECT_CALL(*stream, Finish).WillOnce([&, status] {
    return sequencer.PushBack("Finish").then([status](auto) { return status; });
  });
  EXPECT_CALL(*stream, Cancel).WillRepeatedly([] {});
  return std::unique_ptr<BidiReadStream>(std::move(stream));
}

Status RedirectError(absl::string_view handle, absl::string_view token) {
  auto make_details = [&] {
    auto redirected = google::storage::v2::BidiReadObjectRedirectedError{};
    redirected.mutable_read_handle()->set_handle(std::string(handle));
    redirected.set_routing_token(std::string(token));
    auto details_proto = google::rpc::Status{};
    details_proto.set_code(grpc::StatusCode::ABORTED);
    details_proto.set_message("redirect");
    (void)details_proto.add_details()->PackFrom(redirected);

    std::string details;
    (void)details_proto.SerializeToString(&details);
    return details;
  };

  return google::cloud::MakeStatusFromRpcError(
      grpc::Status(grpc::StatusCode::ABORTED, "redirect", make_details()));
}

// Verify we can open a stream, without retries, timeouts, or any other
// difficulties. This test does not read any data.
TEST(AsyncConnectionImplTest, OpenSimple) {
  auto constexpr kExpectedRequest = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 42
    if_metageneration_match: 7
  )pb";
  auto constexpr kMetadataText = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    name: "test-object"
    generation: 42
  )pb";

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncBidiReadObject)
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    google::cloud::internal::ImmutableOptions const& options) {
        // Verify at least one option is initialized with the correct value.
        EXPECT_EQ(options->get<AuthorityOption>(), kAuthority);

        auto stream = std::make_unique<MockStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
          return sequencer.PushBack("Start").then(
              [](auto f) { return f.get(); });
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [=, &sequencer](
                    google::storage::v2::BidiReadObjectRequest const& request,
                    grpc::WriteOptions) {
                  auto expected = google::storage::v2::BidiReadObjectRequest{};
                  EXPECT_TRUE(TextFormat::ParseFromString(
                      kExpectedRequest, expected.mutable_read_object_spec()));
                  EXPECT_THAT(request, IsProtoEqual(expected));
                  return sequencer.PushBack("Write").then(
                      [](auto f) { return f.get(); });
                });
        EXPECT_CALL(*stream, Read)
            .WillOnce([&]() {
              return sequencer.PushBack("Read").then(
                  [=](auto f) -> std::optional<
                                  google::storage::v2::BidiReadObjectResponse> {
                    if (!f.get()) return std::nullopt;
                    auto constexpr kHandleText = R"pb(
                      handle: "handle-12345"
                    )pb";
                    auto response =
                        google::storage::v2::BidiReadObjectResponse{};
                    EXPECT_TRUE(TextFormat::ParseFromString(
                        kMetadataText, response.mutable_metadata()));
                    EXPECT_TRUE(TextFormat::ParseFromString(
                        kHandleText, response.mutable_read_handle()));
                    return response;
                  });
            })
            .WillOnce([&sequencer]() {
              return sequencer.PushBack("Read[N]").then(
                  [](auto f) -> std::optional<
                                 google::storage::v2::BidiReadObjectResponse> {
                    if (!f.get()) return std::nullopt;
                    return google::storage::v2::BidiReadObjectResponse{};
                  });
            });
        EXPECT_CALL(*stream, Cancel).WillOnce([&sequencer]() {
          (void)sequencer.PushBack("Cancel");
        });
        EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
          return sequencer.PushBack("Finish").then(
              [](auto) { return Status{}; });
        });

        return std::unique_ptr<BidiReadStream>(std::move(stream));
      })
      .WillRepeatedly([](CompletionQueue const&,
                         std::shared_ptr<grpc::ClientContext> const&,
                         google::cloud::internal::ImmutableOptions const&) {
        auto stream = std::make_unique<NiceMock<MockStream>>();
        ON_CALL(*stream, Start).WillByDefault(InvokeWithoutArgs([] {
          return make_ready_future(false);
        }));
        ON_CALL(*stream, Finish).WillByDefault(InvokeWithoutArgs([] {
          return make_ready_future(Status{});
        }));
        ON_CALL(*stream, Cancel).WillByDefault([] {});
        return std::unique_ptr<BidiReadStream>(std::move(stream));
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillRepeatedly([](std::chrono::nanoseconds) {
        return make_ready_future(
            StatusOr<std::chrono::system_clock::time_point>(
                std::chrono::system_clock::now()));
      });
  auto connection = std::make_shared<AsyncConnectionImpl>(
      CompletionQueue(mock_cq), std::shared_ptr<GrpcChannelRefresh>(), mock,
      TestOptions());

  auto request = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto pending = connection->Open({std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto p = pending.get();
  ASSERT_THAT(p, IsOkAndHolds(NotNull()));
  auto descriptor = *std::move(p);

  auto expected_metadata = google::storage::v2::Object{};
  EXPECT_TRUE(TextFormat::ParseFromString(kMetadataText, &expected_metadata));
  EXPECT_THAT(descriptor->metadata(),
              Optional(IsProtoEqual(expected_metadata)));

  // Deleting the descriptor should cancel the stream, and start the background
  // operations to call `Finish()`.
  descriptor.reset();

  auto last_read = sequencer.PopFrontWithName();
  EXPECT_EQ(last_read.second, "Read[N]");
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Cancel");
  next.first.set_value(true);
  last_read.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST(AsyncConnectionImplTest, HandleRedirectErrors) {
  auto constexpr kExpectedRequest0 = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 24
    if_generation_match: 42
  )pb";
  auto constexpr kExpectedRequestN = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 24
    if_generation_match: 42
    read_handle { handle: "test-read-handle" }
    routing_token: "test-routing-token"
  )pb";

  AsyncSequencer<void> sequencer;
  auto make_redirect_stream = [&sequencer](char const* expected_text) {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start").then([](auto) { return true; });
    });
    EXPECT_CALL(*stream, Write)
        .WillOnce([&sequencer, expected_text](
                      google::storage::v2::BidiReadObjectRequest const& r,
                      grpc::WriteOptions) {
          auto expected = google::storage::v2::BidiReadObjectSpec{};
          EXPECT_TRUE(TextFormat::ParseFromString(expected_text, &expected));
          EXPECT_THAT(r.read_object_spec(), IsProtoEqual(expected));
          return sequencer.PushBack("Write").then([](auto) { return true; });
        });
    EXPECT_CALL(*stream, Read).WillOnce([&] {
      return sequencer.PushBack("Read").then([](auto) {
        return std::optional<google::storage::v2::BidiReadObjectResponse>();
      });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) {
        return RedirectError("test-read-handle", "test-routing-token");
      });
    });
    EXPECT_CALL(*stream, Cancel).WillRepeatedly([] {});
    return std::unique_ptr<BidiReadStream>(std::move(stream));
  };

  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncBidiReadObject)
      .WillOnce([&] { return make_redirect_stream(kExpectedRequest0); })
      .WillOnce([&] { return make_redirect_stream(kExpectedRequestN); })
      .WillOnce([&] { return make_redirect_stream(kExpectedRequestN); });

  // Easier to just use a real CQ vs. mock its behavior.
  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = std::make_shared<AsyncConnectionImpl>(
      pool.cq(), std::shared_ptr<GrpcChannelRefresh>(), mock, TestOptions());

  auto request = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest0, &request));
  auto pending = connection->Open({std::move(request), connection->options()});

  for (int i = 0; i != kRetryAttempts + 1; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Start");
    next.first.set_value();

    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Write");
    next.first.set_value();

    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Read");
    next.first.set_value();

    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Finish");
    next.first.set_value();
  }

  ASSERT_THAT(pending.get(), StatusIs(StatusCode::kAborted));
}

TEST(AsyncConnectionImplTest, StopOnPermanentError) {
  ScopedLog log;

  auto constexpr kExpectedRequest = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 24
    if_generation_match: 42
  )pb";

  AsyncSequencer<void> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncBidiReadObject).WillOnce([&]() {
    return MakeErrorStream(sequencer, PermanentError());
  });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  auto connection = std::make_shared<AsyncConnectionImpl>(
      CompletionQueue(mock_cq), std::shared_ptr<GrpcChannelRefresh>(), mock,
      TestOptions());

  auto request = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto pending = connection->Open({std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value();

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value();

  auto response = pending.get();
  ASSERT_THAT(response, StatusIs(PermanentError().code()));

  // A permanent error is never retried, so there is nothing to report.
  EXPECT_THAT(log.ExtractLines(), Not(Contains(HasSubstr("[gcs-retry]"))));
}

TEST(AsyncConnectionImplTest, TooManyTransientErrors) {
  ScopedLog log;

  auto constexpr kExpectedRequest = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 24
    if_generation_match: 42
  )pb";

  AsyncSequencer<void> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncBidiReadObject)
      .Times(kRetryAttempts + 1)
      .WillRepeatedly(
          [&]() { return MakeErrorStream(sequencer, TransientError()); });

  // Easier to just use a real CQ vs. mock its behavior.
  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = std::make_shared<AsyncConnectionImpl>(
      pool.cq(), std::shared_ptr<GrpcChannelRefresh>(), mock, TestOptions());

  auto request = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto pending = connection->Open({std::move(request), connection->options()});

  for (int i = 0; i != kRetryAttempts + 1; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Start");
    next.first.set_value();

    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Finish");
    next.first.set_value();
  }

  ASSERT_THAT(pending.get(), StatusIs(TransientError().code()));

  // `kRetryAttempts` retries are granted, so the first two failures are
  // reported and the third -- the attempt on which the loop gives up -- is
  // not. That last failure reaches the caller as the returned `Status`.
  auto const matches = [](int attempt) {
    return AllOf(HasSubstr("[gcs-retry]"), HasSubstr("Open"),
                 HasSubstr("test-only-invalid/test-object"),
                 HasSubstr("attempt " + std::to_string(attempt)));
  };
  EXPECT_THAT(RetryRecords(log), ElementsAre(matches(1), matches(2)));
}

// A download that succeeds must say nothing, including while the descriptor
// winds down.
//
// When a stream ends cleanly the descriptor re-opens it anyway, because
// `ResumePolicy::OnFinish()` returns `kContinue` for an OK status. That
// re-open routinely loses a race with teardown and fails with an internal
// "stream closed successfully" error, which this retry policy counts as
// transient and retries. Reporting it would put a warning on a read that in
// fact succeeded, so the factory handed to the descriptor does not log.
//
// This is a regression test for real behaviour: against the storage-testbench
// this fired on roughly a third of healthy reads, and on every descriptor
// opened without a range.
//
// Note the resume itself is a different matter. A resume that *fails* is
// still reported, by `ObjectDescriptorImpl::Resume()`; that is covered in
// `object_descriptor_impl_test.cc`. Here every resume ultimately succeeds, so
// there is nothing legitimate to report.
TEST(AsyncConnectionImplTest, ResumeReopenIsSilent) {
  ScopedLog log;
  auto constexpr kExpectedRequest = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
  )pb";

  AsyncSequencer<bool> sequencer;

  // The stream the caller opens on. It serves one response, then ends
  // cleanly, which is what prompts the descriptor to resume.
  auto closing_stream = [] {
    auto stream = std::make_unique<NiceMock<MockStream>>();
    ON_CALL(*stream, Start).WillByDefault(InvokeWithoutArgs([] {
      return make_ready_future(true);
    }));
    ON_CALL(*stream, Write).WillByDefault([](auto const&, auto) {
      return make_ready_future(true);
    });
    ON_CALL(*stream, Read).WillByDefault(InvokeWithoutArgs([n = 0]() mutable {
      using Response = google::storage::v2::BidiReadObjectResponse;
      // The first read satisfies the open; the stream then ends.
      if (n++ == 0) {
        return make_ready_future(std::optional<Response>(Response{}));
      }
      return make_ready_future(std::optional<Response>());
    }));
    ON_CALL(*stream, Finish).WillByDefault(InvokeWithoutArgs([] {
      return make_ready_future(Status{});
    }));
    return std::unique_ptr<BidiReadStream>(std::move(stream));
  };

  // A stream shaped like the one teardown produces: it never starts, and
  // finishing it reports success. `OpenObject` turns that into a transient
  // `kInternal` error, which the retry policy retries.
  auto teardown_stream = [] {
    auto stream = std::make_unique<NiceMock<MockStream>>();
    ON_CALL(*stream, Start).WillByDefault(InvokeWithoutArgs([] {
      return make_ready_future(false);
    }));
    ON_CALL(*stream, Finish).WillByDefault(InvokeWithoutArgs([] {
      return make_ready_future(Status{});
    }));
    return std::unique_ptr<BidiReadStream>(std::move(stream));
  };

  // The stream the resume settles on. It opens and then stays open.
  //
  // It must not end cleanly: `LimitedErrorCountResumePolicy` only counts
  // failures, so an OK finish always answers `kContinue` and a stream that
  // keeps closing cleanly would be resumed forever. Parking the read leaves
  // the descriptor in the state a healthy session is normally in.
  auto parked_stream = [&sequencer] {
    auto stream = std::make_unique<NiceMock<MockStream>>();
    ON_CALL(*stream, Start).WillByDefault(InvokeWithoutArgs([] {
      return make_ready_future(true);
    }));
    ON_CALL(*stream, Write).WillByDefault([](auto const&, auto) {
      return make_ready_future(true);
    });
    ON_CALL(*stream, Read)
        .WillByDefault(InvokeWithoutArgs([&sequencer, n = 0]() mutable {
          using Response = google::storage::v2::BidiReadObjectResponse;
          if (n++ == 0) {
            return make_ready_future(std::optional<Response>(Response{}));
          }
          return sequencer.PushBack("Read").then(
              [](auto) { return std::optional<Response>(); });
        }));
    ON_CALL(*stream, Finish).WillByDefault(InvokeWithoutArgs([] {
      return make_ready_future(Status{});
    }));
    return std::unique_ptr<BidiReadStream>(std::move(stream));
  };

  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncBidiReadObject)
      // `AtLeast(3)` matters: without it the test would pass simply because
      // no resume ever happened, which is the bug it is meant to catch.
      .Times(AtLeast(3))
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    google::cloud::internal::ImmutableOptions const&) {
        return closing_stream();
      })
      // The first re-open loses the race with teardown and fails transiently.
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    google::cloud::internal::ImmutableOptions const&) {
        return teardown_stream();
      })
      // Its retry succeeds, so the resume completes and nothing has gone
      // wrong from the caller's point of view.
      .WillRepeatedly([&](CompletionQueue const&,
                          std::shared_ptr<grpc::ClientContext> const&,
                          google::cloud::internal::ImmutableOptions const&) {
        return parked_stream();
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillRepeatedly([](std::chrono::nanoseconds) {
        return make_ready_future(
            StatusOr<std::chrono::system_clock::time_point>(
                std::chrono::system_clock::now()));
      });
  auto connection = std::make_shared<AsyncConnectionImpl>(
      CompletionQueue(mock_cq), std::shared_ptr<GrpcChannelRefresh>(), mock,
      // Allow a resume, which the default test options switch off.
      TestOptions(Options{}.set<storage::ResumePolicyOption>(
          storage::LimitedErrorCountResumePolicy(1))));

  auto request = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto p = connection->Open({std::move(request), connection->options()}).get();
  ASSERT_THAT(p, IsOkAndHolds(NotNull()));
  auto descriptor = *std::move(p);
  descriptor.reset();

  // Release the parked read so the stream can wind down.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(false);

  EXPECT_THAT(RetryRecords(log), IsEmpty());
}

TEST(AsyncConnectionImplTest, OpenWithReadRanges) {
  auto constexpr kExpectedRequestSpec = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 42
    if_metageneration_match: 7
  )pb";
  auto constexpr kExpectedRequest = R"pb(
    read_object_spec {
      bucket: "test-only-invalid"
      object: "test-object"
      generation: 42
      if_metageneration_match: 7
    }
    read_ranges { read_offset: 0 read_length: 1024 read_id: 1 }
    read_ranges { read_offset: 2048 read_length: 4096 read_id: 2 }
  )pb";
  auto constexpr kMetadataText = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    name: "test-object"
    generation: 42
  )pb";

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncBidiReadObject)
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    google::cloud::internal::ImmutableOptions const& options) {
        EXPECT_EQ(options->get<AuthorityOption>(), kAuthority);

        auto stream = std::make_unique<MockStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
          return sequencer.PushBack("Start").then(
              [](auto f) { return f.get(); });
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [=, &sequencer](
                    google::storage::v2::BidiReadObjectRequest const& request,
                    grpc::WriteOptions) {
                  auto expected = google::storage::v2::BidiReadObjectRequest{};
                  EXPECT_TRUE(
                      TextFormat::ParseFromString(kExpectedRequest, &expected));
                  EXPECT_THAT(request, IsProtoEqual(expected));
                  return sequencer.PushBack("Write").then(
                      [](auto f) { return f.get(); });
                });
        EXPECT_CALL(*stream, Read)
            .WillOnce([&]() {
              return sequencer.PushBack("Read").then(
                  [=](auto f) -> std::optional<
                                  google::storage::v2::BidiReadObjectResponse> {
                    if (!f.get()) return std::nullopt;
                    auto constexpr kHandleText = R"pb(
                      handle: "handle-12345"
                    )pb";
                    auto response =
                        google::storage::v2::BidiReadObjectResponse{};
                    EXPECT_TRUE(TextFormat::ParseFromString(
                        kMetadataText, response.mutable_metadata()));
                    EXPECT_TRUE(TextFormat::ParseFromString(
                        kHandleText, response.mutable_read_handle()));
                    return response;
                  });
            })
            .WillOnce([&sequencer]() {
              return sequencer.PushBack("Read[N]").then(
                  [](auto f) -> std::optional<
                                 google::storage::v2::BidiReadObjectResponse> {
                    if (!f.get()) return std::nullopt;
                    return google::storage::v2::BidiReadObjectResponse{};
                  });
            });
        EXPECT_CALL(*stream, Cancel).WillOnce([&sequencer]() {
          (void)sequencer.PushBack("Cancel");
        });
        EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
          return sequencer.PushBack("Finish").then(
              [](auto) { return Status{}; });
        });

        return std::unique_ptr<BidiReadStream>(std::move(stream));
      })
      .WillRepeatedly([](CompletionQueue const&,
                         std::shared_ptr<grpc::ClientContext> const&,
                         google::cloud::internal::ImmutableOptions const&) {
        auto stream = std::make_unique<NiceMock<MockStream>>();
        ON_CALL(*stream, Start).WillByDefault(InvokeWithoutArgs([] {
          return make_ready_future(false);
        }));
        ON_CALL(*stream, Finish).WillByDefault(InvokeWithoutArgs([] {
          return make_ready_future(Status{});
        }));
        ON_CALL(*stream, Cancel).WillByDefault([] {});
        return std::unique_ptr<BidiReadStream>(std::move(stream));
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillRepeatedly([](std::chrono::nanoseconds) {
        return make_ready_future(
            StatusOr<std::chrono::system_clock::time_point>(
                std::chrono::system_clock::now()));
      });
  auto connection = std::make_shared<AsyncConnectionImpl>(
      CompletionQueue(mock_cq), std::shared_ptr<GrpcChannelRefresh>(), mock,
      TestOptions());

  auto request = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequestSpec, &request));
  auto options =
      connection->options().set<ReadRangesOption>({{0, 1024}, {2048, 4096}});
  auto pending = connection->Open({std::move(request), std::move(options)});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto p = pending.get();
  ASSERT_THAT(p, IsOkAndHolds(NotNull()));
  auto descriptor = *std::move(p);

  descriptor.reset();

  auto last_read = sequencer.PopFrontWithName();
  EXPECT_EQ(last_read.second, "Read[N]");
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Cancel");
  next.first.set_value(true);
  last_read.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

class ScopedMeterProvider {
 public:
  explicit ScopedMeterProvider(
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider)
      : previous_(opentelemetry::metrics::Provider::GetMeterProvider()) {
    opentelemetry::metrics::Provider::SetMeterProvider(provider);
  }
  ~ScopedMeterProvider() {
    opentelemetry::metrics::Provider::SetMeterProvider(previous_);
  }

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
      previous_;
};

std::unique_ptr<ScopedMeterProvider> InstallMockHedgeMetrics(
    int expected_dispatched, int expected_won) {
  using ::google::cloud::testing_util::MockCounter;
  using ::google::cloud::testing_util::MockMeter;
  using ::google::cloud::testing_util::MockMeterProvider;
  using ::testing::_;
  using ::testing::ByMove;
  using ::testing::Eq;
  using ::testing::Return;

  auto dispatched = std::make_unique<MockCounter<std::uint64_t>>();
  auto won = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*dispatched, Add(std::uint64_t{1})).Times(expected_dispatched);
  EXPECT_CALL(*won, Add(std::uint64_t{1})).Times(expected_won);

  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*meter, CreateUInt64Counter(
                          Eq("storage.read_hedging.hedges_dispatched"), _, _))
      .WillOnce(Return(ByMove(opentelemetry::nostd::unique_ptr<
                              opentelemetry::metrics::Counter<std::uint64_t>>(
          dispatched.release()))));
  EXPECT_CALL(*meter,
              CreateUInt64Counter(Eq("storage.read_hedging.hedge_won"), _, _))
      .WillOnce(Return(ByMove(
          opentelemetry::nostd::unique_ptr<
              opentelemetry::metrics::Counter<std::uint64_t>>(won.release()))));

  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter).WillOnce(Return(meter));
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
      global_provider{
          std::shared_ptr<opentelemetry::metrics::MeterProvider>(provider)};
  return std::make_unique<ScopedMeterProvider>(global_provider);
}

future<StatusOr<std::chrono::system_clock::time_point>> MakeSequencedTimer(
    AsyncSequencer<bool>& sequencer, std::string const& name,
    std::atomic<int>& cancelled_timers) {
  future<bool> step = sequencer.PushBack(name);
  auto timer_promise = std::make_shared<
      promise<StatusOr<std::chrono::system_clock::time_point>>>(
      [&cancelled_timers] { ++cancelled_timers; });
  future<StatusOr<std::chrono::system_clock::time_point>> timer_future =
      timer_promise->get_future();
  step.then([timer_promise](future<bool> f) mutable {
    if (!f.get()) {
      timer_promise->set_value(
          internal::CancelledError("timer cancelled", GCP_ERROR_INFO()));
      return;
    }
    timer_promise->set_value(std::chrono::system_clock::now());
  });
  return timer_future;
}

struct ParkedReadState {
  using Response = google::storage::v2::BidiReadObjectResponse;
  std::mutex mu;
  bool cancelled = false;
  bool waiting = false;
  promise<std::optional<Response>> pending;

  future<std::optional<Response>> Park() {
    std::lock_guard<std::mutex> lock(mu);
    if (cancelled || waiting) {
      return make_ready_future(std::optional<Response>());
    }
    waiting = true;
    return pending.get_future();
  }

  void Release() {
    bool wake = false;
    {
      std::lock_guard<std::mutex> lock(mu);
      if (!cancelled) {
        cancelled = true;
        wake = waiting;
      }
    }
    if (wake) pending.set_value(std::nullopt);
  }
};

std::unique_ptr<BidiReadStream> MakeParkedStreamWithStart(
    std::function<future<bool>()> on_start, std::int64_t generation,
    std::atomic<int>* cancel_count) {
  using Response = google::storage::v2::BidiReadObjectResponse;
  auto stream = std::make_unique<NiceMock<MockStream>>();
  auto parked = std::make_shared<ParkedReadState>();
  EXPECT_CALL(*stream, Start).WillOnce(std::move(on_start));
  ON_CALL(*stream, Write)
      .WillByDefault(
          [](google::storage::v2::BidiReadObjectRequest const&,
             grpc::WriteOptions) { return make_ready_future(true); });
  ON_CALL(*stream, Read)
      .WillByDefault(InvokeWithoutArgs([generation, parked, n = 0]() mutable {
        if (n++ == 0) {
          Response response;
          response.mutable_metadata()->set_bucket(
              "projects/_/buckets/test-bucket");
          response.mutable_metadata()->set_name("test-object");
          response.mutable_metadata()->set_generation(generation);
          response.mutable_read_handle()->set_handle("handle-123");
          return make_ready_future(
              std::optional<Response>(std::move(response)));
        }
        return parked->Park();
      }));
  ON_CALL(*stream, Cancel).WillByDefault([parked, cancel_count] {
    if (cancel_count != nullptr) ++(*cancel_count);
    parked->Release();
  });
  ON_CALL(*stream, Finish).WillByDefault(InvokeWithoutArgs([] {
    return make_ready_future(Status{});
  }));
  return std::unique_ptr<BidiReadStream>(std::move(stream));
}

std::unique_ptr<BidiReadStream> MakeImmediateStream(std::int64_t generation) {
  return MakeParkedStreamWithStart([] { return make_ready_future(true); },
                                   generation, nullptr);
}

google::storage::v2::BidiReadObjectSpec MakeTestReadSpec() {
  google::storage::v2::BidiReadObjectSpec spec;
  spec.set_bucket("projects/_/buckets/test-bucket");
  spec.set_object("test-object");
  spec.set_generation(42);
  return spec;
}

class OpenHedgingTest : public ::testing::Test {
 protected:
  using OpenResult =
      StatusOr<std::shared_ptr<storage::ObjectDescriptorConnection>>;
  using OpenFuture = future<OpenResult>;

  void ExpectMetrics(int dispatched, int won) {
    metrics_ = InstallMockHedgeMetrics(dispatched, won);
  }

  void ExpectHedgeTimers(std::vector<std::string> names) {
    if (names.empty()) {
      EXPECT_CALL(*mock_cq_, MakeRelativeTimer).Times(0);
      return;
    }
    std::size_t const count = names.size();
    EXPECT_CALL(*mock_cq_, MakeRelativeTimer)
        .Times(static_cast<int>(count))
        .WillRepeatedly(
            [this, names = std::move(names)](std::chrono::nanoseconds d) {
              std::size_t const idx = timer_delays_.size();
              timer_delays_.push_back(
                  std::chrono::duration_cast<std::chrono::milliseconds>(d));
              std::string const name =
                  idx < names.size() ? names[idx] : std::string("ExtraTimer");
              return MakeSequencedTimer(sequencer_, name, cancelled_timers_);
            });
  }

  promise<bool> PopStep(char const* expected) {
    auto [p, name] = sequencer_.PopFrontWithName();
    EXPECT_EQ(name, expected);
    return std::move(p);
  }

  std::unique_ptr<BidiReadStream> MakeSequencedStream(std::string step,
                                                      std::int64_t generation) {
    return MakeParkedStreamWithStart(
        [this, s = std::move(step)] { return sequencer_.PushBack(s); },
        generation, nullptr);
  }

  std::unique_ptr<BidiReadStream> MakeStalledStream(std::string start_step) {
    return MakeStalledStream(std::move(start_step), "", nullptr);
  }

  std::unique_ptr<BidiReadStream> MakeStalledStream(
      std::string start_step, std::string finish_step,
      std::atomic<int>* cancel_count) {
    auto stream = std::make_unique<NiceMock<MockStream>>();
    EXPECT_CALL(*stream, Start).WillOnce([this, s = std::move(start_step)] {
      return sequencer_.PushBack(s);
    });
    ON_CALL(*stream, Cancel).WillByDefault([cancel_count] {
      if (cancel_count != nullptr) ++(*cancel_count);
    });
    if (!finish_step.empty()) {
      EXPECT_CALL(*stream, Finish).WillOnce([this, f = std::move(finish_step)] {
        return sequencer_.PushBack(f).then(
            [](future<bool>) { return Status{}; });
      });
    } else {
      ON_CALL(*stream, Finish).WillByDefault(InvokeWithoutArgs([] {
        return make_ready_future(Status{});
      }));
    }
    return std::unique_ptr<BidiReadStream>(std::move(stream));
  }

  std::unique_ptr<BidiReadStream> MakeFailingStream(std::string start_step,
                                                    Status status) {
    auto stream = std::make_unique<NiceMock<MockStream>>();
    EXPECT_CALL(*stream, Start).WillOnce([this, s = std::move(start_step)] {
      return sequencer_.PushBack(s).then([](future<bool>) { return false; });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([st = std::move(status)] {
      return make_ready_future(st);
    });
    return std::unique_ptr<BidiReadStream>(std::move(stream));
  }

  std::shared_ptr<AsyncConnectionImpl> MakeConnection(Options opts) {
    opts = internal::MergeOptions(
        std::move(opts),
        Options{}.set<storage::EnableMultiStreamOptimizationOption>(false));
    return std::make_shared<AsyncConnectionImpl>(
        CompletionQueue(mock_cq_), std::shared_ptr<GrpcChannelRefresh>(),
        mock_stub_, TestOptions(std::move(opts)));
  }

  static OpenFuture Open(std::shared_ptr<AsyncConnectionImpl> const& conn,
                         Options opts) {
    return conn->Open({MakeTestReadSpec(), std::move(opts)});
  }

  static OpenFuture Open(std::shared_ptr<AsyncConnectionImpl> const& conn) {
    return Open(conn, conn->options());
  }

  // Every step pushed onto `sequencer_` must be popped and resolved before
  // teardown (un-drained futures terminate in `-fno-exceptions` builds).
  AsyncSequencer<bool> sequencer_;
  std::atomic<int> cancelled_timers_{0};
  std::vector<std::chrono::milliseconds> timer_delays_;
  std::shared_ptr<storage::testing::MockStorageStub> mock_stub_ =
      std::make_shared<storage::testing::MockStorageStub>();
  std::shared_ptr<MockCompletionQueueImpl> mock_cq_ =
      std::make_shared<MockCompletionQueueImpl>();
  std::unique_ptr<ScopedMeterProvider> metrics_;
};

TEST_F(OpenHedgingTest, GatingAndPerCallOverrides) {
  EXPECT_CALL(*mock_stub_, AsyncBidiReadObject).Times(4).WillRepeatedly([] {
    return MakeImmediateStream(42);
  });
  ExpectHedgeTimers({});

  // 1. Disabled by default.
  std::shared_ptr<AsyncConnectionImpl> disabled_conn =
      MakeConnection(Options{});
  OpenResult res1 = Open(disabled_conn).get();
  ASSERT_THAT(res1, IsOkAndHolds(NotNull()));
  res1->reset();

  // 2. Per-call `EnableReadHedgingOption(true)` without client-level opt-in is
  //    a no-op because `hedge_limiter_` is only allocated at client creation.
  Options per_call_enable = disabled_conn->options();
  per_call_enable.set<storage_experimental::EnableReadHedgingOption>(true);
  OpenResult res2 = Open(disabled_conn, std::move(per_call_enable)).get();
  ASSERT_THAT(res2, IsOkAndHolds(NotNull()));
  res2->reset();

  // 3 & 4. Client enabled, suppressed per-call via `EnableReadHedgingOption`
  //        or `MaxReadHedgesOption(0)`.
  std::shared_ptr<AsyncConnectionImpl> enabled_conn = MakeConnection(
      Options{}.set<storage_experimental::EnableReadHedgingOption>(true));
  Options disable_flag = enabled_conn->options();
  disable_flag.set<storage_experimental::EnableReadHedgingOption>(false);
  OpenResult res3 = Open(enabled_conn, std::move(disable_flag)).get();
  ASSERT_THAT(res3, IsOkAndHolds(NotNull()));
  res3->reset();

  Options disable_zero = enabled_conn->options();
  disable_zero.set<storage_experimental::MaxReadHedgesOption>(0);
  OpenResult res4 = Open(enabled_conn, std::move(disable_zero)).get();
  ASSERT_THAT(res4, IsOkAndHolds(NotNull()));
  res4->reset();
}

TEST_F(OpenHedgingTest, FastPrimaryCancelsTimer) {
  ExpectMetrics(0, 0);
  // MSO opens one standby stream after `Open()` succeeds; verify that standby
  // stream does NOT arm a second hedge timer.
  EXPECT_CALL(*mock_stub_, AsyncBidiReadObject)
      .WillOnce([this] { return MakeSequencedStream("PrimaryStart", 101); })
      .WillOnce([] { return MakeImmediateStream(101); });
  ExpectHedgeTimers({"HedgeTimer1"});

  std::shared_ptr<AsyncConnectionImpl> conn = MakeConnection(
      Options{}
          .set<storage::EnableMultiStreamOptimizationOption>(true)
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::ReadHedgeDelayOption>(
              std::chrono::milliseconds(250))
          .set<storage_experimental::MaxReadHedgesOption>(2));

  OpenFuture pending = Open(conn);
  promise<bool> primary_start = PopStep("PrimaryStart");
  promise<bool> timer1 = PopStep("HedgeTimer1");
  EXPECT_THAT(timer_delays_, ElementsAre(std::chrono::milliseconds(250)));

  primary_start.set_value(true);
  OpenResult result = pending.get();
  ASSERT_THAT(result, IsOkAndHolds(NotNull()));
  EXPECT_EQ((*result)->metadata()->generation(), 101);
  EXPECT_EQ(cancelled_timers_.load(), 1);

  timer1.set_value(false);
  result->reset();
}

TEST_F(OpenHedgingTest, SlowPrimaryHedgeWinsAndResumeIsUnhedged) {
  ScopedLog log;
  ExpectMetrics(1, 1);
  std::atomic<int> primary_cancel_count{0};

  EXPECT_CALL(*mock_stub_, AsyncBidiReadObject)
      .WillOnce([this, &primary_cancel_count] {
        return MakeStalledStream("PrimaryStart", "PrimaryFinish",
                                 &primary_cancel_count);
      })
      .WillOnce([this] {
        // Opens with generation 202, then ends cleanly on `HedgeStreamEnd` to
        // trigger a descriptor background resume.
        using Response = google::storage::v2::BidiReadObjectResponse;
        auto stream = std::make_unique<NiceMock<MockStream>>();
        EXPECT_CALL(*stream, Start).WillOnce([this] {
          return sequencer_.PushBack("HedgeStart");
        });
        ON_CALL(*stream, Write)
            .WillByDefault(
                [](google::storage::v2::BidiReadObjectRequest const&,
                   grpc::WriteOptions) { return make_ready_future(true); });
        ON_CALL(*stream, Read)
            .WillByDefault(InvokeWithoutArgs([this, n = 0]() mutable {
              if (n++ == 0) {
                Response response;
                response.mutable_metadata()->set_bucket(
                    "projects/_/buckets/test-bucket");
                response.mutable_metadata()->set_name("test-object");
                response.mutable_metadata()->set_generation(202);
                response.mutable_read_handle()->set_handle("hedge-handle");
                return make_ready_future(
                    std::optional<Response>(std::move(response)));
              }
              return sequencer_.PushBack("HedgeStreamEnd")
                  .then([](future<bool>) { return std::optional<Response>(); });
            }));
        ON_CALL(*stream, Finish).WillByDefault(InvokeWithoutArgs([] {
          return make_ready_future(Status{});
        }));
        return std::unique_ptr<BidiReadStream>(std::move(stream));
      })
      .WillOnce([this] { return MakeSequencedStream("ResumeStart", 202); });
  // Only 2 timers for `max_hedges = 2`; background resume must not arm any.
  ExpectHedgeTimers({"HedgeTimer1", "HedgeTimer2"});

  std::shared_ptr<AsyncConnectionImpl> conn = MakeConnection(
      Options{}
          .set<storage::ResumePolicyOption>(
              storage::LimitedErrorCountResumePolicy(1))
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::ReadHedgeDelayOption>(
              std::chrono::milliseconds(100))
          .set<storage_experimental::MaxReadHedgesOption>(2));

  OpenFuture pending = Open(conn);
  promise<bool> primary_start = PopStep("PrimaryStart");
  promise<bool> timer1 = PopStep("HedgeTimer1");

  timer1.set_value(true);
  promise<bool> hedge_start = PopStep("HedgeStart");
  promise<bool> timer2 = PopStep("HedgeTimer2");

  hedge_start.set_value(true);
  promise<bool> hedge_stream_end = PopStep("HedgeStreamEnd");

  OpenResult result = pending.get();
  ASSERT_THAT(result, IsOkAndHolds(NotNull()));
  EXPECT_EQ((*result)->metadata()->generation(), 202);
  EXPECT_EQ(primary_cancel_count.load(), 1);
  EXPECT_EQ(cancelled_timers_.load(), 1);
  timer2.set_value(false);

  // Cancelled losing primary winds down silently without `[gcs-retry]` logs.
  primary_start.set_value(true);
  PopStep("PrimaryFinish").set_value(true);

  hedge_stream_end.set_value(true);
  PopStep("ResumeStart").set_value(true);

  result->reset();
  EXPECT_THAT(RetryRecords(log), IsEmpty());
}

TEST_F(OpenHedgingTest, PrimaryWinsAfterHedgeDispatched) {
  ExpectMetrics(1, 0);
  std::atomic<int> hedge_cancel_count{0};

  EXPECT_CALL(*mock_stub_, AsyncBidiReadObject)
      .WillOnce([this] { return MakeSequencedStream("PrimaryStart", 1); })
      .WillOnce([this, &hedge_cancel_count] {
        return MakeStalledStream("HedgeStart", "HedgeFinish",
                                 &hedge_cancel_count);
      });
  ExpectHedgeTimers({"HedgeTimer1"});

  std::shared_ptr<AsyncConnectionImpl> conn = MakeConnection(
      Options{}
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::MaxReadHedgesOption>(1));

  OpenFuture pending = Open(conn);
  promise<bool> primary_start = PopStep("PrimaryStart");
  PopStep("HedgeTimer1").set_value(true);
  promise<bool> hedge_start = PopStep("HedgeStart");

  primary_start.set_value(true);
  OpenResult result = pending.get();
  ASSERT_THAT(result, IsOkAndHolds(NotNull()));
  EXPECT_EQ((*result)->metadata()->generation(), 1);
  EXPECT_EQ(hedge_cancel_count.load(), 1);

  hedge_start.set_value(true);
  PopStep("HedgeFinish").set_value(true);
  result->reset();
}

class OpenHedgingRescueTest : public OpenHedgingTest,
                              public ::testing::WithParamInterface<Status> {};

TEST_P(OpenHedgingRescueTest, PrimaryTransientOrStallErrorRescuedByHedge) {
  ExpectMetrics(1, 1);
  EXPECT_CALL(*mock_stub_, AsyncBidiReadObject)
      .WillOnce(
          [this] { return MakeFailingStream("PrimaryStart", GetParam()); })
      .WillOnce([this] { return MakeSequencedStream("HedgeStart", 303); });
  ExpectHedgeTimers({"HedgeTimer1"});

  std::shared_ptr<AsyncConnectionImpl> conn = MakeConnection(
      Options{}
          .set<storage::AsyncRetryPolicyOption>(
              storage::LimitedErrorCountAsyncRetryPolicy(0).clone())
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::MaxReadHedgesOption>(1));

  OpenFuture pending = Open(conn);
  promise<bool> primary_start = PopStep("PrimaryStart");
  PopStep("HedgeTimer1").set_value(true);
  promise<bool> hedge_start = PopStep("HedgeStart");

  primary_start.set_value(false);
  hedge_start.set_value(true);

  OpenResult result = pending.get();
  ASSERT_THAT(result, IsOkAndHolds(NotNull()));
  EXPECT_EQ((*result)->metadata()->generation(), 303);
  result->reset();
}

INSTANTIATE_TEST_SUITE_P(PrimaryErrors, OpenHedgingRescueTest,
                         ::testing::Values(RedirectError("h1", "tok1"),
                                           Status(StatusCode::kCancelled,
                                                  "Stream stall timeout")));

TEST_F(OpenHedgingTest, PrimaryPermanentFailureAbortsInFlightHedge) {
  ExpectMetrics(1, 0);
  std::atomic<int> hedge_cancel_count{0};

  EXPECT_CALL(*mock_stub_, AsyncBidiReadObject)
      .WillOnce([this] {
        return MakeFailingStream("PrimaryStart", PermanentError());
      })
      .WillOnce([this, &hedge_cancel_count] {
        return MakeStalledStream("HedgeStart", "HedgeFinish",
                                 &hedge_cancel_count);
      });
  ExpectHedgeTimers({"HedgeTimer1", "HedgeTimer2"});

  std::shared_ptr<AsyncConnectionImpl> conn = MakeConnection(
      Options{}
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::MaxReadHedgesOption>(2));

  OpenFuture pending = Open(conn);
  promise<bool> primary_start = PopStep("PrimaryStart");
  PopStep("HedgeTimer1").set_value(true);
  promise<bool> hedge_start = PopStep("HedgeStart");
  promise<bool> timer2 = PopStep("HedgeTimer2");

  primary_start.set_value(false);
  EXPECT_THAT(pending.get(), StatusIs(PermanentError().code()));
  EXPECT_EQ(hedge_cancel_count.load(), 1);
  EXPECT_EQ(cancelled_timers_.load(), 1);

  timer2.set_value(false);
  hedge_start.set_value(true);
  PopStep("HedgeFinish").set_value(true);
}

TEST_F(OpenHedgingTest, AllAttemptsFailReturnsPrimaryError) {
  ExpectMetrics(1, 0);
  EXPECT_CALL(*mock_stub_, AsyncBidiReadObject)
      .WillOnce([this] {
        return MakeFailingStream(
            "PrimaryStart",
            Status(StatusCode::kUnavailable, "primary unavailable"));
      })
      .WillOnce([this] {
        return MakeFailingStream(
            "HedgeStart",
            Status(StatusCode::kDeadlineExceeded, "hedge deadline"));
      });
  ExpectHedgeTimers({"HedgeTimer1"});

  std::shared_ptr<AsyncConnectionImpl> conn = MakeConnection(
      Options{}
          .set<storage::AsyncRetryPolicyOption>(
              storage::LimitedErrorCountAsyncRetryPolicy(0).clone())
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::MaxReadHedgesOption>(1));

  OpenFuture pending = Open(conn);
  promise<bool> primary_start = PopStep("PrimaryStart");
  PopStep("HedgeTimer1").set_value(true);
  promise<bool> hedge_start = PopStep("HedgeStart");

  primary_start.set_value(false);
  hedge_start.set_value(false);
  EXPECT_THAT(pending.get(),
              StatusIs(StatusCode::kUnavailable, HasSubstr("primary")));
}

TEST_F(OpenHedgingTest, CallerCancelCancelsAllAttemptsAndTimer) {
  std::atomic<int> cancelled_streams{0};
  EXPECT_CALL(*mock_stub_, AsyncBidiReadObject)
      .WillOnce([this, &cancelled_streams] {
        return MakeStalledStream("PrimaryStart", "PrimaryFinish",
                                 &cancelled_streams);
      })
      .WillOnce([this, &cancelled_streams] {
        return MakeStalledStream("HedgeStart", "HedgeFinish",
                                 &cancelled_streams);
      });
  ExpectHedgeTimers({"HedgeTimer1", "HedgeTimer2"});

  std::shared_ptr<AsyncConnectionImpl> conn = MakeConnection(
      Options{}
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::MaxReadHedgesOption>(2));

  OpenFuture pending = Open(conn);
  promise<bool> primary_start = PopStep("PrimaryStart");
  PopStep("HedgeTimer1").set_value(true);
  promise<bool> hedge_start = PopStep("HedgeStart");
  promise<bool> timer2 = PopStep("HedgeTimer2");

  pending.cancel();
  EXPECT_EQ(cancelled_timers_.load(), 1);
  EXPECT_EQ(cancelled_streams.load(), 2);

  timer2.set_value(false);
  primary_start.set_value(true);
  hedge_start.set_value(true);
  PopStep("PrimaryFinish").set_value(true);
  PopStep("HedgeFinish").set_value(true);

  EXPECT_THAT(pending.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(OpenHedgingTest, RespectsConcurrencyAndLifetimeLimits) {
  using ms = std::chrono::milliseconds;
  ExpectMetrics(2, 2);
  EXPECT_CALL(*mock_stub_, AsyncBidiReadObject)
      .WillOnce([this] { return MakeStalledStream("Call1Primary"); })
      .WillOnce([this] { return MakeStalledStream("Call2Primary"); })
      .WillOnce([this] { return MakeSequencedStream("Call1Hedge", 601); })
      .WillOnce([] { return MakeImmediateStream(602); })
      .WillOnce([] { return MakeImmediateStream(603); });
  // Call 1 arms Timer 1 and (once Hedge 1 starts) Timer 2; Call 2 arms Timer 1,
  // hits `MaxConcurrentHedgesOption(1)` -> re-arms at >= 10ms, then spends the
  // 2nd & final lifetime token so neither Call 2 nor Call 3 arms more timers.
  ExpectHedgeTimers(
      {"Call1Timer1", "Call2Timer1", "Call1Timer2", "Call2RetryTimer"});

  std::shared_ptr<AsyncConnectionImpl> conn = MakeConnection(
      Options{}
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::ReadHedgeDelayOption>(ms(1))
          .set<storage_experimental::MaxReadHedgesOption>(2)
          .set<storage_experimental::MaxConcurrentHedgesOption>(1)
          .set<storage_experimental::MaxTotalHedgesOption>(2));

  OpenFuture call1 = Open(conn);
  OpenFuture call2 = Open(conn);
  promise<bool> c1_primary = PopStep("Call1Primary");
  promise<bool> c1_timer1 = PopStep("Call1Timer1");
  promise<bool> c2_primary = PopStep("Call2Primary");
  promise<bool> c2_timer1 = PopStep("Call2Timer1");

  // Call 1 acquires the single concurrency slot (total 1/2) and arms Timer 2.
  c1_timer1.set_value(true);
  promise<bool> c1_hedge = PopStep("Call1Hedge");
  promise<bool> c1_timer2 = PopStep("Call1Timer2");

  // Call 2 is denied by `MaxConcurrentHedgesOption(1)` and re-arms at 10ms.
  c2_timer1.set_value(true);
  promise<bool> c2_retry = PopStep("Call2RetryTimer");
  EXPECT_THAT(timer_delays_, ElementsAre(ms(1), ms(1), ms(1), ms(10)));

  // Finishing Call 1's hedge releases the concurrency slot before
  // `call1.get()`.
  c1_hedge.set_value(true);
  OpenResult res1 = call1.get();
  ASSERT_THAT(res1, IsOkAndHolds(NotNull()));
  EXPECT_EQ((*res1)->metadata()->generation(), 601);
  c1_timer2.set_value(false);

  // Call 2's retry timer acquires the freed slot and spends total budget (2/2),
  // so no further timer is armed on Call 2 or on subsequent Call 3.
  c2_retry.set_value(true);
  OpenResult res2 = call2.get();
  ASSERT_THAT(res2, IsOkAndHolds(NotNull()));
  EXPECT_EQ((*res2)->metadata()->generation(), 602);

  OpenResult res3 = Open(conn).get();
  ASSERT_THAT(res3, IsOkAndHolds(NotNull()));
  EXPECT_EQ((*res3)->metadata()->generation(), 603);

  c1_primary.set_value(true);
  c2_primary.set_value(true);
  res1->reset();
  res2->reset();
  res3->reset();
}

TEST(AsyncConnectionImplTest, OpenHedgingConcurrentCompletionRace) {
  using OpenResult =
      StatusOr<std::shared_ptr<storage::ObjectDescriptorConnection>>;
  for (int iter = 0; iter != 25; ++iter) {
    std::mutex gate_mu;
    std::condition_variable gate_cv;
    int ready_workers = 0;
    bool release_workers = false;
    std::atomic<int> cancelled_streams{0};
    std::vector<std::thread> workers;
    workers.reserve(2);

    auto const make_racing_stream = [&](std::int64_t generation) {
      auto start_promise = std::make_shared<promise<bool>>();
      auto start_future =
          std::make_shared<future<bool>>(start_promise->get_future());
      workers.emplace_back([&, start_promise]() mutable {
        {
          std::unique_lock<std::mutex> lock(gate_mu);
          ++ready_workers;
          gate_cv.notify_all();
          gate_cv.wait(lock, [&] { return release_workers; });
        }
        start_promise->set_value(true);
      });
      return MakeParkedStreamWithStart(
          [start_future]() mutable { return std::move(*start_future); },
          generation, &cancelled_streams);
    };

    auto mock_stub = std::make_shared<storage::testing::MockStorageStub>();
    EXPECT_CALL(*mock_stub, AsyncBidiReadObject)
        .WillOnce([&] { return make_racing_stream(1); })
        .WillOnce([&] { return make_racing_stream(2); });

    auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
    EXPECT_CALL(*mock_cq, MakeRelativeTimer)
        .WillOnce([](std::chrono::nanoseconds) {
          return make_ready_future(
              StatusOr<std::chrono::system_clock::time_point>(
                  std::chrono::system_clock::now()));
        });

    auto conn = std::make_shared<AsyncConnectionImpl>(
        CompletionQueue(mock_cq), std::shared_ptr<GrpcChannelRefresh>(),
        mock_stub,
        TestOptions(
            Options{}
                .set<storage::EnableMultiStreamOptimizationOption>(false)
                .set<storage_experimental::EnableReadHedgingOption>(true)
                .set<storage_experimental::MaxReadHedgesOption>(1)));

    future<OpenResult> pending =
        conn->Open({MakeTestReadSpec(), conn->options()});
    {
      std::unique_lock<std::mutex> lock(gate_mu);
      gate_cv.wait(lock, [&] { return ready_workers == 2; });
      release_workers = true;
    }
    gate_cv.notify_all();
    for (std::thread& w : workers) w.join();

    OpenResult result = pending.get();
    ASSERT_THAT(result, IsOkAndHolds(NotNull()));
    EXPECT_THAT((*result)->metadata()->generation(),
                ::testing::AnyOf(::testing::Eq(1), ::testing::Eq(2)));
    EXPECT_EQ(cancelled_streams.load(), 1);
    result->reset();
    EXPECT_EQ(cancelled_streams.load(), 2);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
