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
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/match.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <string>
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

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
