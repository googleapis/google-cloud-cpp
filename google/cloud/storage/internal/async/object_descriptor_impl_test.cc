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
using ::testing::ElementsAre;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::ResultOf;
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
  EXPECT_CALL(*stream, Cancel).WillOnce([&sequencer]() {
    sequencer.PushBack("Cancel");
  });

  MockFactory factory;
  EXPECT_CALL(factory, Call).Times(0);
  auto tested = std::make_shared<ObjectDescriptorImpl>(
      NoResume(), factory.AsStdFunction(),
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
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Cancel");
  next.first.set_value(true);
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
  EXPECT_CALL(*stream, Cancel).Times(1);

  MockFactory factory;
  EXPECT_CALL(factory, Call).Times(0);
  auto tested = std::make_shared<ObjectDescriptorImpl>(
      NoResume(), factory.AsStdFunction(),
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
  EXPECT_CALL(*stream, Cancel).Times(1);

  MockFactory factory;
  EXPECT_CALL(factory, Call).Times(0);
  auto tested = std::make_shared<ObjectDescriptorImpl>(
      NoResume(), factory.AsStdFunction(),
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
  EXPECT_CALL(*stream, Cancel).Times(1);

  MockFactory factory;
  EXPECT_CALL(factory, Call).Times(0);
  auto tested = std::make_shared<ObjectDescriptorImpl>(
      NoResume(), factory.AsStdFunction(),
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
  EXPECT_CALL(*stream, Cancel).Times(1);

  MockFactory factory;
  EXPECT_CALL(factory, Call).Times(0);
  auto tested = std::make_shared<ObjectDescriptorImpl>(
      NoResume(), factory.AsStdFunction(),
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
  EXPECT_CALL(*stream, Cancel).Times(1);  // Always called by OpenStream
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

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([=, &sequencer](Request const& request) {
    auto expected = Request{};
    EXPECT_TRUE(TextFormat::ParseFromString(kResumeRequest, &expected));
    EXPECT_THAT(request, IsProtoEqualModuloRepeatedFieldOrdering(expected));
    // Resume with an unrecoverable failure to simplify the test.
    return sequencer.PushBack("Factory").then(
        [&](auto) { return StatusOr<OpenStreamResult>(PermanentError()); });
  });

  auto spec = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kReadSpecText, &spec));
  auto tested = std::make_shared<ObjectDescriptorImpl>(
      storage_experimental::LimitedErrorCountResumePolicy(1)(),
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
    EXPECT_CALL(*stream, Cancel).Times(1);  // Always called by OpenStream
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
  EXPECT_CALL(factory, Call).Times(0);

  auto spec = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kReadSpecText, &spec));
  auto tested = std::make_shared<ObjectDescriptorImpl>(
      NoResume(), factory.AsStdFunction(), spec,
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
    EXPECT_CALL(*stream, Cancel).Times(1);  // Always called by OpenStream
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

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([=, &sequencer](Request const& request) {
    auto expected = Request{};
    EXPECT_TRUE(TextFormat::ParseFromString(kResumeRequest, &expected));
    EXPECT_THAT(request, IsProtoEqualModuloRepeatedFieldOrdering(expected));
    // Resume with an unrecoverable failure to simplify the test.
    return sequencer.PushBack("Factory").then(
        [&](auto) { return StatusOr<OpenStreamResult>(PermanentError()); });
  });

  auto spec = google::storage::v2::BidiReadObjectSpec{};
  ASSERT_TRUE(TextFormat::ParseFromString(kReadSpecText, &spec));
  auto tested = std::make_shared<ObjectDescriptorImpl>(
      storage_experimental::LimitedErrorCountResumePolicy(1)(),
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
  EXPECT_CALL(*stream, Cancel).Times(1);

  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([=, &sequencer](Request const& request) {
    auto expected = Request{};
    EXPECT_TRUE(TextFormat::ParseFromString(kResumeRequest, &expected));
    EXPECT_THAT(request, IsProtoEqualModuloRepeatedFieldOrdering(expected));
    // Resume with an unrecoverable failure to simplify the test.
    return sequencer.PushBack("Factory").then(
        [&](auto) { return StatusOr<OpenStreamResult>(PermanentError()); });
  });

  auto spec = google::storage::v2::BidiReadObjectSpec{};
  EXPECT_TRUE(TextFormat::ParseFromString(kReadSpecText, &spec));
  auto tested = std::make_shared<ObjectDescriptorImpl>(
      NoResume(), factory.AsStdFunction(), spec,
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

/// @test Verify that we can create a subsequent stream and read from it.
TEST(ObjectDescriptorImpl, ReadWithSubsequentStream) {
  // Setup
  auto constexpr kResponse0 = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-12345" }
  )pb";
  auto constexpr kRequest1 = R"pb(
    read_ranges { read_id: 1 read_offset: 100 read_length: 100 }
  )pb";
  auto constexpr kResponse1 = R"pb(
    object_data_ranges {
      range_end: true
      read_range { read_id: 1 read_offset: 100 }
      checksummed_data { content: "payload-for-stream-1" }
    }
  )pb";
  auto constexpr kRequest2 = R"pb(
    read_ranges { read_id: 2 read_offset: 200 read_length: 200 }
  )pb";
  auto constexpr kResponse2 = R"pb(
    object_data_ranges {
      range_end: true
      read_range { read_id: 2 read_offset: 200 }
      checksummed_data { content: "payload-for-stream-2" }
    }
  )pb";

  AsyncSequencer<bool> sequencer;

  // First stream setup
  auto stream1 = std::make_unique<MockStream>();
  EXPECT_CALL(*stream1, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest1, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[1]").then([](auto f) {
          return f.get();
        });
      });
  EXPECT_CALL(*stream1, Read)
      .WillOnce([&]() {
        return sequencer.PushBack("Read[1]").then([&](auto) {
          auto response = Response{};
          EXPECT_TRUE(TextFormat::ParseFromString(kResponse1, &response));
          return absl::make_optional(response);
        });
      })
      .WillOnce([&]() {
        return sequencer.PushBack("Read[1.eos]").then([&](auto) {
          return absl::optional<Response>{};
        });
      });
  EXPECT_CALL(*stream1, Finish).WillOnce([&]() {
    return sequencer.PushBack("Finish[1]").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*stream1, Cancel).Times(1);

  // Second stream setup
  auto stream2 = std::make_unique<MockStream>();
  EXPECT_CALL(*stream2, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions) {
        auto expected = Request{};
        EXPECT_TRUE(TextFormat::ParseFromString(kRequest2, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return sequencer.PushBack("Write[2]").then([](auto f) {
          return f.get();
        });
      });
  EXPECT_CALL(*stream2, Read)
      .WillOnce([&]() {
        return sequencer.PushBack("Read[2]").then([&](auto) {
          auto response = Response{};
          EXPECT_TRUE(TextFormat::ParseFromString(kResponse2, &response));
          return absl::make_optional(response);
        });
      })
      .WillOnce([&]() {
        return sequencer.PushBack("Read[2.eos]").then([](auto) {
          return absl::optional<Response>{};
        });
      });
  EXPECT_CALL(*stream2, Finish).WillOnce([&]() {
    return sequencer.PushBack("Finish[2]").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*stream2, Cancel).Times(1);

  // Mock factory for subsequent streams
  MockFactory factory;
  EXPECT_CALL(factory, Call).WillOnce([&](Request const&) {
    auto stream_result = OpenStreamResult{
        std::make_shared<OpenStream>(std::move(stream2)), Response{}};
    return make_ready_future(make_status_or(std::move(stream_result)));
  });

  // Create the ObjectDescriptorImpl
  auto tested = std::make_shared<ObjectDescriptorImpl>(
      NoResume(), factory.AsStdFunction(),
      google::storage::v2::BidiReadObjectSpec{},
      std::make_shared<OpenStream>(std::move(stream1)));

  auto response0 = Response{};
  EXPECT_TRUE(TextFormat::ParseFromString(kResponse0, &response0));
  tested->Start(std::move(response0));

  auto read1 = sequencer.PopFrontWithName();
  EXPECT_EQ(read1.second, "Read[1]");
  // Start a read on the first stream
  auto reader1 = tested->Read({100, 100});
  auto future1 = reader1->Read();
  // The implementation starts a read loop eagerly after Start(), and then
  // the call to tested->Read() schedules a write.
  auto write1 = sequencer.PopFrontWithName();
  EXPECT_EQ(write1.second, "Write[1]");
  write1.first.set_value(true);

  // Now we can satisfy the read. This will deliver the data to the reader.
  read1.first.set_value(true);

  EXPECT_THAT(future1.get(),
              VariantWith<storage_experimental::ReadPayload>(ResultOf(
                  "contents are",
                  [](storage_experimental::ReadPayload const& p) {
                    return p.contents();
                  },
                  ElementsAre(absl::string_view{"payload-for-stream-1"}))));

  EXPECT_THAT(reader1->Read().get(), VariantWith<Status>(IsOk()));

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read[1.eos]");
  next.first.set_value(true);

  // The first stream should be finishing now.
  auto finish1 = sequencer.PopFrontWithName();
  EXPECT_EQ(finish1.second, "Finish[1]");
  finish1.first.set_value(true);

  // Create and switch to a new stream
  tested->MakeSubsequentStream();

  auto read2 = sequencer.PopFrontWithName();
  EXPECT_EQ(read2.second, "Read[2]");
  // Start a read on the second stream
  auto reader2 = tested->Read({200, 200});
  auto future2 = reader2->Read();

  auto write2 = sequencer.PopFrontWithName();
  EXPECT_EQ(write2.second, "Write[2]");
  write2.first.set_value(true);

  read2.first.set_value(true);

  EXPECT_THAT(future2.get(),
              VariantWith<storage_experimental::ReadPayload>(ResultOf(
                  "contents are",
                  [](storage_experimental::ReadPayload const& p) {
                    return p.contents();
                  },
                  ElementsAre(absl::string_view{"payload-for-stream-2"}))));

  EXPECT_THAT(reader2->Read().get(), VariantWith<Status>(IsOk()));

  auto read2_eos = sequencer.PopFrontWithName();
  EXPECT_EQ(read2_eos.second, "Read[2.eos]");
  read2_eos.first.set_value(true);

  auto finish2 = sequencer.PopFrontWithName();
  EXPECT_EQ(finish2.second, "Finish[2]");
  finish2.first.set_value(true);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
