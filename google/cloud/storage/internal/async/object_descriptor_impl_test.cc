// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/object_descriptor_impl.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/async/resume_policy.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/string_view.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/storage/v2/storage.pb.h>
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AtMost;
using ::testing::ElementsAre;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::VariantWith;

using Request = google::storage::v2::BidiReadObjectRequest;
using Response = google::storage::v2::BidiReadObjectResponse;
using MockStream =
    google::cloud::mocks::MockAsyncStreamingReadWriteRpc<Request, Response>;
using MockFactory =
    ::testing::MockFunction<future<StatusOr<OpenStreamResult>>(Request)>;

auto constexpr kMetadataText = R"pb(
  bucket: "projects/_/buckets/test-bucket"
  name: "test-object"
  generation: 42
)pb";

auto NoResume() {
  return storage_experimental::LimitedErrorCountResumePolicy(0)();
}

auto MakeTested(
    std::unique_ptr<storage_experimental::ResumePolicy> resume_policy,
    OpenStreamFactory make_stream,
    google::storage::v2::BidiReadObjectSpec read_object_spec,
    std::shared_ptr<OpenStream> stream) {
  Options options;
  options.set<storage_experimental::EnableMultiStreamOptimizationOption>(true);
  return std::make_shared<ObjectDescriptorImpl>(
      std::move(resume_policy), std::move(make_stream),
      std::move(read_object_spec), std::move(stream), std::move(options));
}

MATCHER_P(IsProtoEqualModuloRepeatedFieldOrdering, value,
          "Checks whether protos are equal, ignoring repeated field ordering") {
  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  differencer.ReportDifferencesToString(&delta);
  if (differencer.Compare(arg, value)) return true;
  *result_listener << "\n" << delta;
  return false;
}

/// @test Verify opening a stream and closing it produces the expected results.
TEST(ObjectDescriptorImpl, LifecycleNoRead) {
  AsyncSequencer<bool> sequencer;
  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Read).WillOnce([&sequencer]() {
    return sequencer.PushBack("Read[1]").then(
        [](auto) { return absl::optional<Response>{}; });
  });
  EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then(
        [](auto) { return PermanentError(); });
  });
  EXPECT_CALL(*stream, Cancel).Times(AtMost(1));

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([](Request const&) {
    return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
  });
  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream)));
  auto response = Response{};
  EXPECT_TRUE(
      TextFormat::ParseFromString(kMetadataText, response.mutable_metadata()));
  tested->Start(std::move(response));
  EXPECT_TRUE(tested->metadata().has_value());

  auto expected_metadata = google::storage::v2::Object{};
  EXPECT_TRUE(TextFormat::ParseFromString(kMetadataText, &expected_metadata));
  EXPECT_THAT(tested->metadata(), Optional(IsProtoEqual(expected_metadata)));

  auto read1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read1.second, "Read[1]");
  read1.first.set_value(true);

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  tested.reset();
}

/// @test Verify that Cancel() is called if OnFinish() is delayed.
TEST(ObjectDescriptorImpl, LifecycleCancelRacesWithFinish) {
  AsyncSequencer<bool> sequencer;
  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Read).WillOnce([&sequencer]() {
    return sequencer.PushBack("Read[1]").then(
        [](auto) { return absl::optional<Response>{}; });
  });
  EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then(
        [](auto) { return PermanentError(); });
  });
  // This time we expect Cancel() because we will delay OnFinish().
  EXPECT_CALL(*stream, Cancel).Times(1);

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([](Request const&) {
    return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
  });
  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream)));
  auto response = Response{};
  EXPECT_TRUE(
      TextFormat::ParseFromString(kMetadataText, response.mutable_metadata()));
  tested->Start(std::move(response));
  EXPECT_TRUE(tested->metadata().has_value());

  auto expected_metadata = google::storage::v2::Object{};
  EXPECT_TRUE(TextFormat::ParseFromString(kMetadataText, &expected_metadata));
  EXPECT_THAT(tested->metadata(), Optional(IsProtoEqual(expected_metadata)));

  auto read1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read1.second, "Read[1]");
  read1.first.set_value(true);

  // This pops the future that OnFinish() waits for.
  auto finish = sequencer.PopFrontWithName();
  EXPECT_EQ(finish.second, "Finish");

  // Reset the descriptor *before* OnFinish() gets to run. This invokes the
  // destructor, which calls Cancel() on any streams that have not yet been
  // removed by OnFinish().
  tested.reset();

  // Now allow OnFinish() to run.
  finish.first.set_value(true);
}

/// @test Read a single stream and then close.
TEST(ObjectDescriptorImpl, ReadSingleRange) {
  auto constexpr kResponse0 = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-12345" }
  )pb";

  auto constexpr kRequest1 = R"pb(
    read_ranges { read_id: 1 read_offset: 20000 read_length: 100 }
  )pb";
  auto constexpr kResponse1 = R"pb(
    read_handle { handle: "handle-23456" }
    object_data_ranges {
      range_end: true
      read_range { read_id: 1 read_offset: 20000 }
      checksummed_data {
        content: "The quick brown fox jumps over the lazy dog"
      }
    }
  )pb";

  AsyncSequencer<bool> sequencer;
  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest1, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[1]").then([](auto f) {
          return f.get();
        });
      });
  EXPECT_CALL(*stream, Read)
      .WillOnce([=, &sequencer]() {
        return sequencer.PushBack("Read[1]").then([&](auto) {
          auto response = Response{};
          EXPECT_TRUE(TextFormat::ParseFromString(kResponse1, &response));
          return absl::make_optional(response);
        });
      })
      .WillOnce([&sequencer]() {
        return sequencer.PushBack("Read[2]").then(
            [](auto) { return absl::optional<Response>{}; });
      });
  EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then(
        [](auto) { return PermanentError(); });
  });
  EXPECT_CALL(*stream, Cancel).Times(AtMost(1));

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([](Request const&) {
    return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
  });
  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream)));
  auto response = Response{};
  EXPECT_TRUE(TextFormat::ParseFromString(kResponse0, &response));
  tested->Start(std::move(response));
  EXPECT_TRUE(tested->metadata().has_value());

  auto read1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read1.second, "Read[1]");

  auto expected_metadata = google::storage::v2::Object{};
  EXPECT_TRUE(TextFormat::ParseFromString(kMetadataText, &expected_metadata));
  EXPECT_THAT(tested->metadata(), Optional(IsProtoEqual(expected_metadata)));

  auto s1 = tested->Read({20000, 100});
  auto s1r1 = s1->Read();

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write[1]");
  next.first.set_value(true);
  read1.first.set_value(true);

  // The future returned by `Read()` should become satisfied at this point.
  // We expect it to contain the right data.
  EXPECT_THAT(s1r1.get(),
              VariantWith<storage_experimental::ReadPayload>(ResultOf(
                  "contents are",
                  [](storage_experimental::ReadPayload const& p) {
                    return p.contents();
                  },
                  ElementsAre(absl::string_view{
                      "The quick brown fox jumps over the lazy dog"}))));
  // Since the `range_end()` flag is set, we expect the stream to finish with
  // success.
  EXPECT_THAT(s1->Read().get(), VariantWith<Status>(IsOk()));

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read[2]");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

/// @test Reading multiple ranges creates a single request.
TEST(ObjectDescriptorImpl, ReadMultipleRanges) {
  auto constexpr kLength = 100;
  auto constexpr kOffset = 20000;
  auto constexpr kRequest1 = R"pb(
    read_ranges { read_id: 1 read_offset: 20000 read_length: 100 }
  )pb";
  auto constexpr kRequest2 = R"pb(
    read_ranges { read_id: 2 read_offset: 40000 read_length: 100 }
    read_ranges { read_id: 3 read_offset: 60000 read_length: 100 }
  )pb";
  auto constexpr kResponse0 = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-12345" }
  )pb";
  auto constexpr kResponse1 = R"pb(
    object_data_ranges {
      range_end: true
      read_range { read_id: 1 read_offset: 20000 }
      checksummed_data {
        content: "The quick brown fox jumps over the lazy dog"
      }
    }
  )pb";

  AsyncSequencer<bool> sequencer;
  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest1, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[1]").then([](auto f) {
          return f.get();
        });
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest2, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[2]").then([](auto f) {
          return f.get();
        });
      });

  EXPECT_CALL(*stream, Read)
      .WillOnce([&]() {
        return sequencer.PushBack("Read[1]").then([&](auto) {
          // Simulate a response with 3 out of order messages.
          auto response = Response{};
          EXPECT_TRUE(TextFormat::ParseFromString(kResponse1, &response));
          return absl::make_optional(response);
        });
      })
      .WillRepeatedly([&]() {
        return sequencer.PushBack("Read[2]").then(
            [&](auto) { return absl::optional<Response>{}; });
      });

  EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then(
        [](auto) { return PermanentError(); });
  });
  EXPECT_CALL(*stream, Cancel).Times(AtMost(1));

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([](Request const&) {
    return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
  });
  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream)));
  auto response = Response{};
  EXPECT_TRUE(TextFormat::ParseFromString(kResponse0, &response));
  tested->Start(std::move(response));
  EXPECT_TRUE(tested->metadata().has_value());

  auto read1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read1.second, "Read[1]");

  auto s1 = tested->Read({kOffset, kLength});
  ASSERT_THAT(s1, NotNull());

  // Asking for data should result in an immediate `Write()` message with the
  // first range.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write[1]");

  // Additional ranges are queued until the first `Write()` call is completed.
  auto s2 = tested->Read({2 * kOffset, kLength});
  ASSERT_THAT(s2, NotNull());

  auto s3 = tested->Read({3 * kOffset, kLength});
  ASSERT_THAT(s3, NotNull());

  // Complete the first `Write()` call, that should result in a second
  // `Write()` call with the two additional ranges.
  next.first.set_value(true);

  // And then the follow up `Write()` message with the queued information.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write[2]");
  next.first.set_value(true);

  auto s1r1 = s1->Read();
  auto s2r1 = s2->Read();
  auto s3r1 = s3->Read();
  EXPECT_FALSE(s1r1.is_ready());
  EXPECT_FALSE(s2r1.is_ready());
  EXPECT_FALSE(s3r1.is_ready());

  read1.first.set_value(true);

  // The future returned by `Read()` should become satisfied at this point.
  // We expect it to contain the right data.
  EXPECT_THAT(s1r1.get(),
              VariantWith<storage_experimental::ReadPayload>(ResultOf(
                  "contents are",
                  [](storage_experimental::ReadPayload const& p) {
                    return p.contents();
                  },
                  ElementsAre(absl::string_view{
                      "The quick brown fox jumps over the lazy dog"}))));
  // Since the `range_end()` flag is set, we expect the stream to finish with
  // success.
  EXPECT_THAT(s1->Read().get(), VariantWith<Status>(IsOk()));

  // Simulate a clean shutdown with an unrecoverable error.
  auto last_read = sequencer.PopFrontWithName();
  EXPECT_EQ(last_read.second, "Read[2]");
  last_read.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  EXPECT_TRUE(s2r1.is_ready());
  EXPECT_TRUE(s3r1.is_ready());
  EXPECT_THAT(s2r1.get(), VariantWith<Status>(PermanentError()));
  EXPECT_THAT(s3r1.get(), VariantWith<Status>(PermanentError()));
}

/// @test Reading a range may require many messages.
TEST(ObjectDescriptorImpl, ReadSingleRangeManyMessages) {
  auto constexpr kRequest1 = R"pb(
    read_ranges { read_id: 1 read_offset: 20000 read_length: 100 }
  )pb";
  auto constexpr kResponse0 = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-12345" }
  )pb";
  auto constexpr kResponse1 = R"pb(
    object_data_ranges {
      range_end: false
      read_range { read_id: 1 read_offset: 20000 }
      checksummed_data {
        content: "The quick brown fox jumps over the lazy dog"
      }
    }
  )pb";
  auto constexpr kResponse2 = R"pb(
    object_data_ranges {
      range_end: true
      read_range { read_id: 1 read_offset: 20026 }
      checksummed_data {
        content: "The quick brown fox jumps over the lazy dog"
      }
    }
  )pb";
  auto constexpr kOffset = 20000;
  auto constexpr kLength = 100;

  AsyncSequencer<bool> sequencer;
  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest1, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[1]").then([](auto f) {
          return f.get();
        });
      });

  EXPECT_CALL(*stream, Read)
      .WillOnce([&]() {
        return sequencer.PushBack("Read[1]").then([&](auto) {
          auto response = Response{};
          EXPECT_TRUE(TextFormat::ParseFromString(kResponse1, &response));
          return absl::make_optional(response);
        });
      })
      .WillOnce([&]() {
        return sequencer.PushBack("Read[2]").then([&](auto) {
          auto response = Response{};
          EXPECT_TRUE(TextFormat::ParseFromString(kResponse2, &response));
          return absl::make_optional(response);
        });
      })
      .WillRepeatedly([&]() {
        return sequencer.PushBack("Read[3]").then(
            [&](auto) { return absl::optional<Response>{}; });
      });

  EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then(
        [](auto) { return PermanentError(); });
  });
  EXPECT_CALL(*stream, Cancel).Times(AtMost(1));

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([](Request const&) {
    return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
  });
  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream)));
  auto response = Response{};
  EXPECT_TRUE(TextFormat::ParseFromString(kResponse0, &response));
  tested->Start(std::move(response));
  EXPECT_TRUE(tested->metadata().has_value());

  auto read = sequencer.PopFrontWithName();
  EXPECT_EQ(read.second, "Read[1]");

  auto s1 = tested->Read({kOffset, kLength});
  ASSERT_THAT(s1, NotNull());

  // Asking for data should result in an immediate `Write()` message with the
  // first range.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write[1]");
  next.first.set_value(true);

  auto s1r1 = s1->Read();
  EXPECT_FALSE(s1r1.is_ready());

  read.first.set_value(true);

  // The future returned by `Read()` should become satisfied at this point.
  // We expect it to contain the right data.
  EXPECT_THAT(s1r1.get(),
              VariantWith<storage_experimental::ReadPayload>(ResultOf(
                  "contents are",
                  [](storage_experimental::ReadPayload const& p) {
                    return p.contents();
                  },
                  ElementsAre(absl::string_view{
                      "The quick brown fox jumps over the lazy dog"}))));

  auto s1r2 = s1->Read();
  EXPECT_FALSE(s1r2.is_ready());

  read = sequencer.PopFrontWithName();
  EXPECT_EQ(read.second, "Read[2]");
  read.first.set_value(true);

  // The future returned by `Read()` should become satisfied at this point.
  // We expect it to contain the right data.
  EXPECT_THAT(s1r2.get(),
              VariantWith<storage_experimental::ReadPayload>(ResultOf(
                  "contents are",
                  [](storage_experimental::ReadPayload const& p) {
                    return p.contents();
                  },
                  ElementsAre(absl::string_view{
                      "The quick brown fox jumps over the lazy dog"}))));

  // Since the `range_end()` flag is set, we expect the stream to finish with
  // success.
  EXPECT_THAT(s1->Read().get(), VariantWith<Status>(IsOk()));

  auto last_read = sequencer.PopFrontWithName();
  EXPECT_EQ(last_read.second, "Read[3]");
  last_read.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

/// @test When the underlying stream fails with unrecoverable errors all ranges
/// fail.
TEST(ObjectDescriptorImpl, AllRangesFailOnUnrecoverableError) {
  auto constexpr kLength = 100;
  auto constexpr kOffset = 20000;
  auto constexpr kRequest1 = R"pb(
    read_ranges { read_id: 1 read_offset: 20000 read_length: 100 }
  )pb";
  auto constexpr kRequest2 = R"pb(
    read_ranges { read_id: 2 read_offset: 40000 read_length: 100 }
    read_ranges { read_id: 3 read_offset: 60000 read_length: 100 }
  )pb";

  AsyncSequencer<bool> sequencer;
  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest1, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[1]").then([](auto f) {
          return f.get();
        });
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest2, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[2]").then([](auto f) {
          return f.get();
        });
      });

  EXPECT_CALL(*stream, Read).WillOnce([&]() {
    return sequencer.PushBack("Read[1]").then(
        [](auto) { return absl::optional<Response>{}; });
  });

  EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then(
        [](auto) { return PermanentError(); });
  });
  EXPECT_CALL(*stream, Cancel).Times(AtMost(1));

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([](Request const&) {
    return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
  });
  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream)));
  tested->Start(Response{});
  EXPECT_FALSE(tested->metadata().has_value());

  auto read = sequencer.PopFrontWithName();
  EXPECT_EQ(read.second, "Read[1]");

  auto s1 = tested->Read({kOffset, kLength});
  ASSERT_THAT(s1, NotNull());

  // Asking for data should result in an immediate `Write()` message with the
  // first range.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write[1]");

  // Additional ranges are queued until the first `Write()` call is completed.
  auto s2 = tested->Read({2 * kOffset, kLength});
  ASSERT_THAT(s2, NotNull());

  auto s3 = tested->Read({3 * kOffset, kLength});
  ASSERT_THAT(s3, NotNull());

  // Complete the first `Write()` call, that should result in a second
  // `Write()` call with the two additional ranges.
  next.first.set_value(true);

  // And then the follow up `Write()` message with the queued information.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write[2]");
  next.first.set_value(true);

  auto s1r1 = s1->Read();
  auto s2r1 = s2->Read();
  auto s3r1 = s3->Read();
  EXPECT_FALSE(s1r1.is_ready());
  EXPECT_FALSE(s2r1.is_ready());
  EXPECT_FALSE(s3r1.is_ready());

  // Simulate a failure.
  read.first.set_value(false);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  // All the ranges fail with the same error.
  EXPECT_THAT(s1r1.get(), VariantWith<Status>(PermanentError()));
  EXPECT_THAT(s2r1.get(), VariantWith<Status>(PermanentError()));
  EXPECT_THAT(s3r1.get(), VariantWith<Status>(PermanentError()));
}

auto InitialStream(AsyncSequencer<bool>& sequencer) {
  auto constexpr kRequest1 = R"pb(
    read_ranges { read_id: 1 read_offset: 20000 read_length: 100 }
  )pb";
  auto constexpr kRequest2 = R"pb(
    read_ranges { read_id: 2 read_offset: 40000 read_length: 100 }
    read_ranges { read_id: 3 read_offset: 60000 read_length: 100 }
  )pb";

  auto constexpr kResponse1 = R"pb(
    object_data_ranges {
      read_range { read_id: 1 read_offset: 20000 }
      checksummed_data { content: "0123456789" crc32c: 0x280c069e }
    }
    object_data_ranges {
      read_range { read_id: 2 read_offset: 40000 }
      checksummed_data { content: "0123456789" crc32c: 0x280c069e }
    }
    object_data_ranges {
      range_end: true
      read_range { read_id: 3 read_offset: 40000 }
      checksummed_data { content: "0123456789" crc32c: 0x280c069e }
    }
  )pb";

  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Cancel).Times(AtMost(1));  // Always called by OpenStream
  EXPECT_CALL(*stream, Write)
      .WillOnce([=, &sequencer](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest1, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[1]").then([](auto f) {
          return f.get();
        });
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest2, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[2]").then([](auto f) {
          return f.get();
        });
      });

  EXPECT_CALL(*stream, Read)
      .WillOnce([=, &sequencer]() {
        return sequencer.PushBack("Read[1]").then([&](auto) {
          auto response = Response{};
          EXPECT_TRUE(TextFormat::ParseFromString(kResponse1, &response));
          return absl::make_optional(response);
        });
      })
      .WillOnce([&]() {
        return sequencer.PushBack("Read[2]").then(
            [](auto) { return absl::optional<Response>{}; });
      });

  EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then(
        [](auto) { return TransientError(); });
  });

  return stream;
}

/// @test Verify that resuming a stream adjusts all offsets.
TEST(ObjectDescriptorImpl, ResumeRangesOnRecoverableError) {
// There is a problem with this test and certain versions of MSVC (19.43.34808)
// skipping it on WIN32 for now.
#ifdef _WIN32
  GTEST_SKIP();
#endif

  auto constexpr kLength = 100;
  auto constexpr kOffset = 20000;
  auto constexpr kReadSpecText = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 24
    if_generation_match: 42
  )pb";
  // The resume request should include all the remaining ranges, starting from
  // the remaining offset (10 bytes after the start).
  auto constexpr kResumeRequest = R"pb(
    read_object_spec {
      bucket: "test-only-invalid"
      object: "test-object"
      generation: 24
      if_generation_match: 42
      read_handle { handle: "handle-12345" }
    }
    read_ranges { read_id: 1 read_offset: 20010 read_length: 90 }
    read_ranges { read_id: 2 read_offset: 40010 read_length: 90 }
  )pb";
  auto constexpr kResponse0 = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-12345" }
  )pb";

  AsyncSequencer<bool> sequencer;
  Request expected_resume_request;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kResumeRequest, &expected_resume_request));

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](Request const& request) {
        EXPECT_TRUE(request.read_ranges().empty());
        return make_ready_future(StatusOr<OpenStreamResult>(TransientError()));
      })
      .WillOnce([&](Request const& request) {
        EXPECT_THAT(request, IsProtoEqualModuloRepeatedFieldOrdering(
                                 expected_resume_request));
        // Resume with an unrecoverable failure to simplify the test.
        return sequencer.PushBack("Factory").then(
            [](auto) { return StatusOr<OpenStreamResult>(PermanentError()); });
      });

  auto spec = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kReadSpecText, &spec));
  auto tested =
      MakeTested(storage_experimental::LimitedErrorCountResumePolicy(1)(),
                 factory.AsStdFunction(), spec,
                 std::make_shared<OpenStream>(InitialStream(sequencer)));
  auto response = Response{};
  EXPECT_TRUE(TextFormat::ParseFromString(kResponse0, &response));
  tested->Start(std::move(response));
  EXPECT_TRUE(tested->metadata().has_value());

  auto read1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read1.second, "Read[1]");

  auto s1 = tested->Read({kOffset, kLength});
  ASSERT_THAT(s1, NotNull());

  // Asking for data should result in an immediate `Write()` message with the
  // first range.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write[1]");

  // Additional ranges are queued until the first `Write()` call is completed.
  auto s2 = tested->Read({2 * kOffset, kLength});
  ASSERT_THAT(s2, NotNull());

  auto s3 = tested->Read({3 * kOffset, kLength});
  ASSERT_THAT(s3, NotNull());

  // Complete the first `Write()` call, that should result in a second
  // `Write()` call with the two additional ranges.
  next.first.set_value(true);

  // And then the follow up `Write()` message with the queued information.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write[2]");
  next.first.set_value(true);

  auto s1r1 = s1->Read();
  auto s2r1 = s2->Read();
  auto s3r1 = s3->Read();
  EXPECT_FALSE(s1r1.is_ready());
  EXPECT_FALSE(s2r1.is_ready());
  EXPECT_FALSE(s3r1.is_ready());

  // Simulate a partial read.
  read1.first.set_value(true);
  // The ranges should have some data.
  EXPECT_TRUE(s1r1.is_ready());
  EXPECT_TRUE(s2r1.is_ready());
  EXPECT_TRUE(s3r1.is_ready());

  auto expected_r1 = VariantWith<storage_experimental::ReadPayload>(ResultOf(
      "contents are",
      [](storage_experimental::ReadPayload const& p) { return p.contents(); },
      ElementsAre(absl::string_view{"0123456789"})));

  EXPECT_THAT(s1r1.get(), expected_r1);
  EXPECT_THAT(s2r1.get(), expected_r1);
  EXPECT_THAT(s3r1.get(), expected_r1);

  auto s1r2 = s1->Read();
  auto s2r2 = s2->Read();
  auto s3r2 = s3->Read();
  EXPECT_FALSE(s1r2.is_ready());
  EXPECT_FALSE(s2r2.is_ready());
  // The third range should be fully done.
  EXPECT_TRUE(s3r2.is_ready());
  EXPECT_THAT(s3r2.get(), VariantWith<Status>(IsOk()));

  // Simulate the recoverable failure.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read[2]");
  next.first.set_value(false);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Factory");
  next.first.set_value(true);

  // All the ranges fail with the same error.
  EXPECT_THAT(s1r2.get(), VariantWith<Status>(PermanentError()));
  EXPECT_THAT(s2r2.get(), VariantWith<Status>(PermanentError()));
}

Status RedirectError(absl::string_view handle, absl::string_view token) {
  auto details = [&] {
    auto redirected = google::storage::v2::BidiReadObjectRedirectedError{};
    redirected.mutable_read_handle()->set_handle(std::string(handle));
    redirected.set_routing_token(std::string(token));
    auto details_proto = google::rpc::Status{};
    details_proto.set_code(grpc::StatusCode::UNAVAILABLE);
    details_proto.set_message("redirect");
    details_proto.add_details()->PackFrom(redirected);

    std::string details;
    details_proto.SerializeToString(&details);
    return details;
  };

  return google::cloud::MakeStatusFromRpcError(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "redirect", details()));
}

/// @test Verify that resuming a stream uses a handle and routing token.
TEST(ObjectDescriptorImpl, PendingFinish) {
  auto constexpr kResponse0 = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-12345" }
  )pb";

  AsyncSequencer<bool> sequencer;

  auto initial_stream = [&sequencer]() {
    auto constexpr kRequest1 = R"pb(
      read_ranges { read_id: 1 read_offset: 20000 read_length: 100 }
    )pb";

    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Cancel)
        .Times(AtMost(1));  // Always called by OpenStream
    EXPECT_CALL(*stream, Write)
        .WillOnce([=, &sequencer](Request const& request, grpc::WriteOptions) {
          auto expected = Request{};
          EXPECT_TRUE(TextFormat::ParseFromString(kRequest1, &expected));
          EXPECT_THAT(request, IsProtoEqual(expected));
          return sequencer.PushBack("Write[1]").then([](auto) {
            return false;
          });
        });

    EXPECT_CALL(*stream, Read).WillOnce([&]() {
      return sequencer.PushBack("Read[1]").then(
          [](auto) { return absl::optional<Response>{}; });
    });

    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then(
          [](auto) { return TransientError(); });
    });

    return stream;
  };

  auto constexpr kLength = 100;
  auto constexpr kOffset = 20000;
  auto constexpr kReadSpecText = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 24
    if_generation_match: 42
  )pb";

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([](Request const&) {
    return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
  });

  auto spec = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kReadSpecText, &spec));
  auto tested = MakeTested(NoResume(), factory.AsStdFunction(), spec,
                           std::make_shared<OpenStream>(initial_stream()));
  auto response = Response{};
  EXPECT_TRUE(TextFormat::ParseFromString(kResponse0, &response));
  tested->Start(std::move(response));
  EXPECT_TRUE(tested->metadata().has_value());

  auto read1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read1.second, "Read[1]");

  auto s1 = tested->Read({kOffset, kLength});
  ASSERT_THAT(s1, NotNull());

  // Asking for data should result in an immediate `Write()` message with the
  // first range.
  auto write1 = sequencer.PopFrontWithName();
  EXPECT_EQ(write1.second, "Write[1]");

  // Simulate a (nearly) simultaneous error in the `Write()` and `Read()` calls.
  read1.first.set_value(false);
  write1.first.set_value(false);
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  // The ranges fails with the same error.
  EXPECT_THAT(s1->Read().get(), VariantWith<Status>(TransientError()));
}

/// @test Verify that resuming a stream uses a handle and routing token.
TEST(ObjectDescriptorImpl, ResumeUsesRouting) {
  auto constexpr kResponse0 = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-12345" }
  )pb";

  AsyncSequencer<bool> sequencer;

  auto initial_stream = [&sequencer]() {
    auto constexpr kRequest1 = R"pb(
      read_ranges { read_id: 1 read_offset: 20000 read_length: 100 }
    )pb";

    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Cancel).Times(AtMost(1));
    EXPECT_CALL(*stream, Write)
        .WillOnce([=, &sequencer](Request const& request, grpc::WriteOptions) {
          auto expected = Request{};
          EXPECT_TRUE(TextFormat::ParseFromString(kRequest1, &expected));
          EXPECT_THAT(request, IsProtoEqual(expected));
          return sequencer.PushBack("Write[1]").then([](auto f) {
            return f.get();
          });
        });

    EXPECT_CALL(*stream, Read).WillOnce([&]() {
      return sequencer.PushBack("Read[1]").then(
          [](auto) { return absl::optional<Response>{}; });
    });

    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then([](auto) {
        return RedirectError("handle-redirect-3456", "token-redirect-3456");
      });
    });

    return stream;
  };

  auto constexpr kLength = 100;
  auto constexpr kOffset = 20000;
  auto constexpr kReadSpecText = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 24
    if_generation_match: 42
  )pb";
  // The resume request should include all the remaining ranges, starting from
  // the remaining offset (10 bytes after the start).
  auto constexpr kResumeRequest = R"pb(
    read_object_spec {
      bucket: "test-only-invalid"
      object: "test-object"
      generation: 24
      if_generation_match: 42
      read_handle { handle: "handle-redirect-3456" }
      routing_token: "token-redirect-3456"
    }
    read_ranges { read_id: 1 read_offset: 20000 read_length: 100 }
  )pb";

  Request expected_resume_request;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kResumeRequest, &expected_resume_request));

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](Request const& request) {
        EXPECT_TRUE(request.read_ranges().empty());
        return make_ready_future(StatusOr<OpenStreamResult>(TransientError()));
      })
      .WillOnce([&](Request const& request) {
        EXPECT_THAT(request, IsProtoEqualModuloRepeatedFieldOrdering(
                                 expected_resume_request));
        // Resume with an unrecoverable failure to simplify the test.
        return sequencer.PushBack("Factory").then(
            [](auto) { return StatusOr<OpenStreamResult>(PermanentError()); });
      });

  auto spec = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kReadSpecText, &spec));
  auto tested =
      MakeTested(storage_experimental::LimitedErrorCountResumePolicy(1)(),
                 factory.AsStdFunction(), spec,
                 std::make_shared<OpenStream>(initial_stream()));
  auto response = Response{};
  EXPECT_TRUE(TextFormat::ParseFromString(kResponse0, &response));
  tested->Start(std::move(response));
  EXPECT_TRUE(tested->metadata().has_value());

  auto read1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read1.second, "Read[1]");

  auto s1 = tested->Read({kOffset, kLength});
  ASSERT_THAT(s1, NotNull());

  // Asking for data should result in an immediate `Write()` message with the
  // first range.
  auto write1 = sequencer.PopFrontWithName();
  EXPECT_EQ(write1.second, "Write[1]");

  // Simulate the recoverable failure.
  read1.first.set_value(false);
  write1.first.set_value(false);
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Factory");
  next.first.set_value(true);

  // The ranges fails with the same error.
  EXPECT_THAT(s1->Read().get(), VariantWith<Status>(PermanentError()));
}

Status PartialFailure(std::int64_t read_id) {
  auto details = [&] {
    auto error = google::storage::v2::BidiReadObjectError{};
    auto& range_error = *error.add_read_range_errors();
    range_error.set_read_id(read_id);
    range_error.mutable_status()->set_code(grpc::StatusCode::INVALID_ARGUMENT);
    range_error.mutable_status()->set_message("out of range read");

    auto details_proto = google::rpc::Status{};
    details_proto.set_code(grpc::StatusCode::INVALID_ARGUMENT);
    details_proto.set_message("some reads are out of range");
    details_proto.add_details()->PackFrom(error);

    std::string details;
    details_proto.SerializeToString(&details);
    return details;
  };

  return google::cloud::MakeStatusFromRpcError(
      grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "redirect", details()));
}

/// @test When the underlying stream fails with unrecoverable errors all ranges
/// fail.
TEST(ObjectDescriptorImpl, RecoverFromPartialFailure) {
  auto constexpr kLength = 100;
  auto constexpr kOffset = 20000;
  auto constexpr kReadSpecText = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 24
    if_generation_match: 42
  )pb";
  auto constexpr kRequest1 = R"pb(
    read_ranges { read_id: 1 read_offset: 20000 read_length: 100 }
  )pb";
  auto constexpr kRequest2 = R"pb(
    read_ranges { read_id: 2 read_offset: 4000000 read_length: 100 }
    read_ranges { read_id: 3 read_offset: 60000 read_length: 100 }
  )pb";

  // The resume request should include all the remaining ranges.
  auto constexpr kResumeRequest = R"pb(
    read_object_spec {
      bucket: "test-only-invalid"
      object: "test-object"
      generation: 24
      if_generation_match: 42
    }
    read_ranges { read_id: 1 read_offset: 20000 read_length: 100 }
    read_ranges { read_id: 3 read_offset: 60000 read_length: 100 }
  )pb";

  AsyncSequencer<bool> sequencer;
  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest1, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[1]").then([](auto f) {
          return f.get();
        });
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest2, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[2]").then([](auto f) {
          return f.get();
        });
      });

  EXPECT_CALL(*stream, Read).WillOnce([&]() {
    return sequencer.PushBack("Read[1]").then(
        [](auto) { return absl::optional<Response>{}; });
  });

  EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then([](auto) {
      // Return an error, indicating that range #2 is invalid. It should
      // resume with the new ranges.
      return PartialFailure(2);
    });
  });
  EXPECT_CALL(*stream, Cancel).Times(AtMost(1));

  Request expected_resume_request;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kResumeRequest, &expected_resume_request));
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](Request const& request) {
        EXPECT_TRUE(request.read_ranges().empty());
        return make_ready_future(StatusOr<OpenStreamResult>(TransientError()));
      })
      .WillOnce([&](Request const& request) {
        EXPECT_THAT(request, IsProtoEqualModuloRepeatedFieldOrdering(
                                 expected_resume_request));
        // Resume with an unrecoverable failure to simplify the test.
        return sequencer.PushBack("Factory").then(
            [](auto) { return StatusOr<OpenStreamResult>(PermanentError()); });
      });

  auto spec = google::storage::v2::BidiReadObjectSpec{};
  EXPECT_TRUE(TextFormat::ParseFromString(kReadSpecText, &spec));
  auto tested = MakeTested(NoResume(), factory.AsStdFunction(), spec,
                           std::make_shared<OpenStream>(std::move(stream)));
  tested->Start(Response{});
  EXPECT_FALSE(tested->metadata().has_value());

  auto read = sequencer.PopFrontWithName();
  EXPECT_EQ(read.second, "Read[1]");

  auto s1 = tested->Read({kOffset, kLength});
  ASSERT_THAT(s1, NotNull());

  // Asking for data should result in an immediate `Write()` message with the
  // first range.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write[1]");

  // Additional ranges are queued until the first `Write()` call is completed.
  auto s2 = tested->Read({2 * kOffset * 100, kLength});
  ASSERT_THAT(s2, NotNull());

  auto s3 = tested->Read({3 * kOffset, kLength});
  ASSERT_THAT(s3, NotNull());

  // Complete the first `Write()` call, that should result in a second
  // `Write()` call with the two additional ranges.
  next.first.set_value(true);

  // And then the follow up `Write()` message with the queued information.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write[2]");
  next.first.set_value(true);

  auto s1r1 = s1->Read();
  auto s2r1 = s2->Read();
  auto s3r1 = s3->Read();
  EXPECT_FALSE(s1r1.is_ready());
  EXPECT_FALSE(s2r1.is_ready());
  EXPECT_FALSE(s3r1.is_ready());

  // Simulate a failure.
  read.first.set_value(false);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  // Range 2 should fail with the invalid argument error.
  EXPECT_THAT(s2r1.get(),
              VariantWith<Status>(StatusIs(StatusCode::kInvalidArgument)));

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Factory");
  next.first.set_value(true);

  // All the other ranges fail with the same error.
  EXPECT_THAT(s1r1.get(), VariantWith<Status>(PermanentError()));
  EXPECT_THAT(s3r1.get(), VariantWith<Status>(PermanentError()));
}

/// @test Verify that a background stream is created proactively.
TEST(ObjectDescriptorImpl, ProactiveStreamCreation) {
  AsyncSequencer<bool> sequencer;
  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Read).WillOnce([&] {
    return sequencer.PushBack("Read").then(
        [](auto) { return absl::optional<Response>(); });
  });
  EXPECT_CALL(*stream, Finish).WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*stream, Cancel).Times(AtMost(1));  // Always called by OpenStream

  MockFactory factory;
  // This is the proactive stream creation. The implementation is designed to
  // always keep a pending stream in flight, so it may call the factory more
  // than once if a pending stream is consumed or fails. We use WillRepeatedly
  // to make the test robust to this behavior.
  EXPECT_CALL(factory, Call).WillRepeatedly([&](Request const& request) {
    EXPECT_TRUE(request.read_ranges().empty());
    return sequencer.PushBack("Factory").then(
        [](auto) { return StatusOr<OpenStreamResult>(PermanentError()); });
  });

  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream)));

  tested->Start(Response{});

  // In the implementation, the Read() is started before the factory call.
  auto read_called = sequencer.PopFrontWithName();
  EXPECT_EQ(read_called.second, "Read");

  auto factory_called = sequencer.PopFrontWithName();
  EXPECT_EQ(factory_called.second, "Factory");

  // Allow the events to complete.
  read_called.first.set_value(true);
  factory_called.first.set_value(true);
}

/// @test Verify a new stream is used if all existing streams are busy.
TEST(ObjectDescriptorImpl, MakeSubsequentStreamCreatesNewWhenAllBusy) {
  AsyncSequencer<bool> sequencer;

  // Setup for the first stream, which will remain busy.
  auto stream1 = std::make_unique<MockStream>();
  EXPECT_CALL(*stream1, Write(_, _))
      .Times(::testing::AtMost(1))
      .WillRepeatedly(
          [](Request const&, auto) { return make_ready_future(true); });
  // Keep stream1 busy but return ready futures.
  EXPECT_CALL(*stream1, Read).WillRepeatedly([] {
    return make_ready_future(absl::optional<Response>{});
  });
  EXPECT_CALL(*stream1, Finish).WillOnce([] {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*stream1, Cancel).Times(AtMost(1));

  // Setup for the second stream, which will be created proactively.
  auto stream2 = std::make_unique<MockStream>();
  EXPECT_CALL(*stream2, Write(_, _))
      .Times(::testing::AtMost(1))
      .WillRepeatedly([](...) { return make_ready_future(true); });
  EXPECT_CALL(*stream2, Read).WillRepeatedly([] {
    return make_ready_future(absl::optional<Response>{});
  });
  EXPECT_CALL(*stream2, Finish).WillRepeatedly([] {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*stream2, Cancel).Times(AtMost(1));

  // First call is proactive for stream2. It may be called more than once.
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([&](Request const&) {
        return make_ready_future(StatusOr<OpenStreamResult>(OpenStreamResult{
            std::make_shared<OpenStream>(std::move(stream2)), Response{}}));
      })
      .WillRepeatedly([](Request const&) {
        return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
      });

  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream1)));
  tested->Start(Response{});

  // Start a read on stream1 to make it busy.
  auto reader1 = tested->Read({0, 100});

  // Call MakeSubsequentStream. Since stream1 is busy, this should activate the
  // pending stream (stream2).
  tested->MakeSubsequentStream();

  // A new read should now be routed to stream2.
  auto reader2 = tested->Read({100, 200});

  // stream2 returns ready futures; no sequencer pop needed here.
  tested->Cancel();
  tested.reset();
}

/// @test Verify reusing an idle stream that is already last is a no-op.
TEST(ObjectDescriptorImpl, MakeSubsequentStreamReusesIdleStreamAlreadyLast) {
  AsyncSequencer<bool> sequencer;

  // Setup for the first and only stream.
  auto stream1 = std::make_unique<MockStream>();
  EXPECT_CALL(*stream1, Write(_, _))
      .Times(AtMost(1))
      .WillRepeatedly(
          [](auto const&, auto) { return make_ready_future(true); });

  EXPECT_CALL(*stream1, Read)
      .WillOnce([&] {  // From Start()
        return sequencer.PushBack("Read[1.1]").then([](auto) {
          auto resp = Response{};
          auto* r = resp.add_object_data_ranges();
          r->set_range_end(true);
          r->mutable_read_range()->set_read_id(0);
          r->mutable_read_range()->set_read_offset(0);
          return absl::make_optional(std::move(resp));
        });
      })
      .WillOnce([&] {  // From OnRead() loop
        return sequencer.PushBack("Read[1.2]").then([](auto) {
          return absl::make_optional(Response{});
        });
      })
      .WillOnce([] {  // Complete read_id=1
        auto resp = Response{};
        auto* r = resp.add_object_data_ranges();
        r->set_range_end(true);
        r->mutable_read_range()->set_read_id(1);
        r->mutable_read_range()->set_read_offset(0);
        return make_ready_future(absl::make_optional(std::move(resp)));
      })
      .WillRepeatedly(
          [] { return make_ready_future(absl::optional<Response>{}); });

  // Finish() will be called by the OpenStream destructor.
  EXPECT_CALL(*stream1, Finish).WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*stream1, Cancel)
      .Times(AtMost(1));  // Always called by OpenStream

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](Request const&) {
        return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
      })
      .WillRepeatedly([](Request const&) {
        return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
      });

  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream1)));
  auto start_response = Response{};
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(object_data_ranges { read_range { read_id: 0 read_offset: 0 } })pb",
      &start_response));
  tested->Start(std::move(start_response));

  auto read1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read1.second, "Read[1.1]");
  read1.first.set_value(true);

  auto read2 = sequencer.PopFrontWithName();
  EXPECT_EQ(read2.second, "Read[1.2]");
  read2.first.set_value(true);

  // Call MakeSubsequentStream. It should find stream1 and return.
  tested->MakeSubsequentStream();

  // Ensure background activity stops cleanly.
  tested->Cancel();
  tested.reset();
}

/// @test Verify an idle stream at the front is moved to the back and reused.
TEST(ObjectDescriptorImpl, MakeSubsequentStreamReusesAndMovesIdleStream) {
  AsyncSequencer<bool> sequencer;

  // First stream setup
  auto stream1 = std::make_unique<MockStream>();
  EXPECT_CALL(*stream1, Write(_, _)).WillRepeatedly([](auto const&, auto) {
    return make_ready_future(true);
  });

  EXPECT_CALL(*stream1, Read)
      .WillOnce([&] {  // From Start()
        return sequencer.PushBack("Read[1.1]").then([](auto) {
          return absl::make_optional(Response{});
        });
      })
      .WillOnce([] {  // From OnRead() loop
        auto response = Response{};
        auto* r = response.add_object_data_ranges();
        r->set_range_end(true);
        r->mutable_read_range()->set_read_id(1);
        r->mutable_read_range()->set_read_offset(0);
        return make_ready_future(absl::make_optional(std::move(response)));
      })
      .WillOnce([] {  // From OnRead() loop after reader1 done
        return make_ready_future(absl::make_optional(Response{}));
      })
      .WillRepeatedly(
          [] { return make_ready_future(absl::optional<Response>{}); });
  EXPECT_CALL(*stream1, Finish).WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*stream1, Cancel).Times(AtMost(1));

  // Second stream setup
  auto stream2 = std::make_unique<MockStream>();
  EXPECT_CALL(*stream2, Write(_, _))
      .WillOnce([&](Request const&, auto) {
        return sequencer.PushBack("Write[2.1]").then([](auto f) {
          return f.get();
        });
      })
      .WillRepeatedly(
          [](auto const&, auto) { return make_ready_future(true); });
  EXPECT_CALL(*stream2, Read)
      .WillOnce([&] {
        return sequencer.PushBack("Read[2.1]").then([](auto) {
          auto resp = Response{};
          auto* r = resp.add_object_data_ranges();
          r->set_range_end(true);
          r->mutable_read_range()->set_read_id(2);
          r->mutable_read_range()->set_read_offset(100);
          return absl::make_optional(std::move(resp));
        });
      })
      .WillRepeatedly(
          [] { return make_ready_future(absl::optional<Response>{}); });
  EXPECT_CALL(*stream2, Finish).WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*stream2, Cancel).Times(AtMost(1));

  // Third stream setup
  auto stream3 = std::make_unique<MockStream>();
  // stream3 sits in pending_stream_ and may become active; provide ready
  // defaults.
  EXPECT_CALL(*stream3, Write(_, _)).WillRepeatedly([](auto const&, auto) {
    return make_ready_future(true);
  });
  EXPECT_CALL(*stream3, Read).WillRepeatedly([] {
    return make_ready_future(absl::optional<Response>{});
  });
  // stream3 sits in pending_stream_ and is destroyed when the test ends.
  // It is never "started", so Finish() is never called.
  EXPECT_CALL(*stream3, Finish).Times(AtMost(1)).WillRepeatedly([] {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*stream3, Cancel).Times(AtMost(1));

  // Mock factory for subsequent streams
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([&] {  // For stream2 (Triggered by Start)
        auto resp = Response{};
        EXPECT_TRUE(
            TextFormat::ParseFromString(
                R"pb(object_data_ranges {
                       read_range { read_id: 0 read_offset: 0 }
                     })pb",
                &resp));
        return make_ready_future(StatusOr<OpenStreamResult>(
            OpenStreamResult{std::make_shared<OpenStream>(std::move(stream2)),
                             std::move(resp)}));
      })
      .WillOnce([&] {  // For stream3 (Triggered by MakeSubsequentStream)
        return make_ready_future(StatusOr<OpenStreamResult>(OpenStreamResult{
            std::make_shared<OpenStream>(std::move(stream3)), Response{}}));
      })
      .WillRepeatedly([&](Request const&) {
        return make_ready_future(StatusOr<OpenStreamResult>(PermanentError()));
      });

  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream1)));

  auto start_response = Response{};
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(object_data_ranges { read_range { read_id: 0 read_offset: 0 } })pb",
      &start_response));
  tested->Start(std::move(start_response));

  auto read1_1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read1_1.second, "Read[1.1]");

  // Make stream1 busy
  auto reader1 = tested->Read({0, 100});

  // Create and switch to a new stream. This happens before the first
  // stream is finished.
  tested->MakeSubsequentStream();

  auto read2_1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read2_1.second, "Read[2.1]");

  // Make stream2 busy
  auto reader2 = tested->Read({100, 200});
  auto write2_1 = sequencer.PopFrontWithName();
  EXPECT_EQ(write2_1.second, "Write[2.1]");
  write2_1.first.set_value(true);  // Complete write immediately

  // Make stream1 IDLE.
  auto r1f1 = reader1->Read();
  read1_1.first.set_value(true);
  // read1_2/read1_3 completed via ready futures; no sequencer pops needed.
  ASSERT_THAT(r1f1.get(), VariantWith<storage_experimental::ReadPayload>(_));
  auto r1f2 = reader1->Read();
  ASSERT_THAT(r1f2.get(), VariantWith<Status>(IsOk()));

  // Call MakeSubsequentStream. It finds stream1, moves it, and returns.
  tested->MakeSubsequentStream();

  auto reader3 = tested->Read({200, 300});
  read2_1.first.set_value(true);
  tested.reset();
}

/// @test Verify that a successful resume executes the OnResume logic correctly.
TEST(ObjectDescriptorImpl, OnResumeSuccessful) {
  AsyncSequencer<bool> sequencer;

  auto expect_startup_events = [&](AsyncSequencer<bool>& seq) {
    auto e1 = seq.PopFrontWithName();
    auto e2 = seq.PopFrontWithName();
    std::set<std::string> names = {e1.second, e2.second};
    if (names.count("Read[1]") != 0 && names.count("ProactiveFactory") != 0) {
      e1.first.set_value(true);  // Allow read to proceed
      e2.first.set_value(true);  // Allow factory to proceed
    } else {
      ADD_FAILURE() << "Got unexpected events: " << e1.second << ", "
                    << e2.second;
    }
  };

  auto stream1 = std::make_unique<MockStream>();
  EXPECT_CALL(*stream1, Write).WillOnce([&](auto, auto) {
    return sequencer.PushBack("Write[1]").then([](auto f) { return f.get(); });
  });

  // To keep Stream 1 alive during startup, the first Read returns a valid
  // (empty) response. Subsequent reads return nullopt to trigger the
  // Finish/Resume logic.
  EXPECT_CALL(*stream1, Read)
      .WillOnce([&] {
        return sequencer.PushBack("Read[1]").then(
            [](auto) { return absl::make_optional(Response{}); });
      })
      .WillRepeatedly([&] {
        return sequencer.PushBack("Read[Loop]").then([](auto) {
          return absl::optional<Response>{};
        });
      });

  EXPECT_CALL(*stream1, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish[1]").then([](auto) {
      return TransientError();
    });
  });
  EXPECT_CALL(*stream1, Cancel).Times(AtMost(1));

  auto stream2 = std::make_unique<MockStream>();
  // The resumed stream to starts reading immediately.
  EXPECT_CALL(*stream2, Read).WillRepeatedly([&] {
    return sequencer.PushBack("Read[2]").then(
        [](auto) { return absl::make_optional(Response{}); });
  });
  EXPECT_CALL(*stream2, Finish).WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*stream2, Cancel).Times(AtMost(1));

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([&](auto) {
        return sequencer.PushBack("ProactiveFactory").then([](auto) {
          return StatusOr<OpenStreamResult>(TransientError());
        });
      })
      .WillOnce([&](auto) {
        return sequencer.PushBack("ResumeFactory").then([&](auto) {
          return StatusOr<OpenStreamResult>(OpenStreamResult{
              std::make_shared<OpenStream>(std::move(stream2)), Response{}});
        });
      });

  auto tested = MakeTested(
      storage_experimental::LimitedErrorCountResumePolicy(1)(),
      factory.AsStdFunction(), google::storage::v2::BidiReadObjectSpec{},
      std::make_shared<OpenStream>(std::move(stream1)));

  tested->Start(Response{});
  expect_startup_events(sequencer);

  // Register the read range.
  auto reader = tested->Read({0, 100});

  auto next_event = sequencer.PopFrontWithName();
  promise<bool> fail_stream_promise;

  if (next_event.second == "Read[Loop]") {
    fail_stream_promise = std::move(next_event.first);
    // Now expect Write[1]
    auto w1 = sequencer.PopFrontWithName();
    EXPECT_EQ(w1.second, "Write[1]");
    w1.first.set_value(true);
  } else {
    // It was Write[1] immediately
    EXPECT_EQ(next_event.second, "Write[1]");
    next_event.first.set_value(true);

    // Now wait for Read[Loop]
    auto read_loop = sequencer.PopFrontWithName();
    EXPECT_EQ(read_loop.second, "Read[Loop]");
    fail_stream_promise = std::move(read_loop.first);
  }

  // Trigger Failure on Stream 1.
  fail_stream_promise.set_value(true);

  auto f1 = sequencer.PopFrontWithName();
  EXPECT_EQ(f1.second, "Finish[1]");
  f1.first.set_value(true);

  auto resume = sequencer.PopFrontWithName();
  EXPECT_EQ(resume.second, "ResumeFactory");
  resume.first.set_value(true);

  // The OnResume block calls OnRead, which triggers Read() on Stream 2.
  auto r2 = sequencer.PopFrontWithName();
  EXPECT_EQ(r2.second, "Read[2]");
  r2.first.set_value(true);

  tested.reset();
}

/// @test Verify Read() behavior when all streams have failed permanently.
TEST(ObjectDescriptorImpl, ReadFailsWhenAllStreamsAreDead) {
  AsyncSequencer<bool> sequencer;
  auto stream = std::make_unique<MockStream>();

  // Initial Read returns empty (EOF) to trigger Finish
  EXPECT_CALL(*stream, Read).WillOnce([&] {
    return sequencer.PushBack("Read[1]").then(
        [](auto) { return absl::optional<Response>{}; });
  });

  EXPECT_CALL(*stream, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then(
        [](auto) { return PermanentError(); });
  });
  EXPECT_CALL(*stream, Cancel).Times(AtMost(1));

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([&](auto) {
    return sequencer.PushBack("ProactiveFactory").then([](auto) {
      return StatusOr<OpenStreamResult>(PermanentError());
    });
  });

  auto tested = MakeTested(NoResume(), factory.AsStdFunction(),
                           google::storage::v2::BidiReadObjectSpec{},
                           std::make_shared<OpenStream>(std::move(stream)));

  tested->Start(Response{});

  auto e1 = sequencer.PopFrontWithName();
  auto e2 = sequencer.PopFrontWithName();

  std::set<std::string> names = {e1.second, e2.second};
  ASSERT_EQ(names.count("Read[1]"), 1);
  ASSERT_EQ(names.count("ProactiveFactory"), 1);

  e1.first.set_value(true);
  e2.first.set_value(true);

  auto finish = sequencer.PopFrontWithName();
  EXPECT_EQ(finish.second, "Finish");
  finish.first.set_value(true);

  // At this point, the only active stream failed permanently and was removed.
  // The proactive stream creation also failed. The manager is now EMPTY.

  auto reader = tested->Read({0, 100});

  // The Read() should immediately fail with FailedPrecondition.
  auto result = reader->Read().get();
  EXPECT_THAT(result,
              VariantWith<Status>(StatusIs(StatusCode::kFailedPrecondition)));
}

/// @test Verify that disabling multi-stream optimization prevents background
/// stream creation.
TEST(ObjectDescriptorImpl, MultiStreamOptimizationDisabled) {
  AsyncSequencer<bool> sequencer;
  auto stream = std::make_unique<MockStream>();

  EXPECT_CALL(*stream, Read).WillOnce([&] {
    return sequencer.PushBack("Read").then(
        [](auto) { return absl::optional<Response>(); });
  });
  EXPECT_CALL(*stream, Finish).WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*stream, Cancel).Times(AtMost(1));

  MockFactory factory;

  Options options;
  options.set<storage_experimental::EnableMultiStreamOptimizationOption>(false);

  auto tested = std::make_shared<ObjectDescriptorImpl>(
      NoResume(), factory.AsStdFunction(),
      google::storage::v2::BidiReadObjectSpec{},
      std::make_shared<OpenStream>(std::move(stream)), options);

  tested->Start(Response{});

  auto read_called = sequencer.PopFrontWithName();
  EXPECT_EQ(read_called.second, "Read");
  read_called.first.set_value(true);

  tested->MakeSubsequentStream();

  EXPECT_EQ(tested->StreamSize(), 1);

  tested.reset();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
