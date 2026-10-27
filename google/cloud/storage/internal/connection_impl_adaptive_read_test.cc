// Copyright 2026 Google LLC
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

#include "google/cloud/storage/hashing_options.h"
#include "google/cloud/storage/internal/adaptive_object_read_source.h"
#include "google/cloud/storage/internal/adaptive_read_size_estimator.h"
#include "google/cloud/storage/internal/connection_impl.h"
#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/storage/internal/object_read_streambuf.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_generic_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_cat.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <future>
#include <istream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockGenericStub;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Not;
using ::testing::Optional;
using ::testing::SizeIs;

std::int64_t constexpr kGeneration = 1234;

Options TestOptions() {
  using ms = std::chrono::milliseconds;
  return Options{}
      .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(3).clone())
      .set<BackoffPolicyOption>(
          ExponentialBackoffPolicy(ms(1), ms(2), 2).clone())
      .set<IdempotencyPolicyOption>(AlwaysRetryIdempotencyPolicy{}.clone())
      // Each test has its own history, so the first windows do not depend on
      // the tests that ran before.
      .set<AdaptiveReadSizeEstimatorOption>(
          std::make_shared<AdaptiveReadSizeEstimator>());
}

// Small windows keep the test objects small.
storage_experimental::AdaptiveReadRangeConfig TestConfig() {
  storage_experimental::AdaptiveReadRangeConfig config;
  config.initial_window = 1024;
  config.max_window = 4096;
  config.prefetch_threshold = 0.0;
  return config;
}

std::string MakeContents(std::int64_t size) {
  std::string contents(static_cast<std::size_t>(size), '\0');
  std::uint32_t state = 7;
  std::generate(contents.begin(), contents.end(), [&state] {
    state = state * 1664525U + 1013904223U;
    return static_cast<char>(state >> 24);
  });
  return contents;
}

/// What the stub received.
struct RecordedRequest {
  // The value of the `Range` header, empty for a download of the full object.
  std::string range;
  std::optional<std::int64_t> generation;
  std::optional<std::int64_t> if_metageneration_match;
  std::optional<std::int64_t> if_metageneration_not_match;
  // The request belongs to an adaptive read.
  bool tagged;
  std::thread::id thread;
};

MATCHER_P(RangeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.range, result_listener);
}

MATCHER_P(GenerationIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.generation, result_listener);
}

MATCHER_P(IfMetagenerationMatchIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.if_metageneration_match,
                            result_listener);
}

MATCHER_P(IfMetagenerationNotMatchIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.if_metageneration_not_match,
                            result_listener);
}

MATCHER_P(IsTagged, matcher, "") {
  return ExplainMatchResult(matcher, arg.tagged, result_listener);
}

/**
 * Serves the requested range of a fake object, like the REST stub does.
 *
 * The headers arrive with the first read, and the source closes after the last
 * byte. If @p fail_after is set, the source fails with a transient error after
 * returning that many bytes. With @p transcode the response emulates
 * decompressive transcoding: the service ignored the range, and the response
 * reports a transformation.
 */
class FakeRangeSource : public ObjectReadSource {
 public:
  FakeRangeSource(std::string const& contents, HashValues hashes,
                  std::int64_t begin, std::int64_t end,
                  std::optional<std::int64_t> fail_after, bool transcode)
      : contents_(contents),
        hashes_(std::move(hashes)),
        begin_(begin),
        end_(end),
        next_(begin),
        fail_after_(fail_after),
        transcode_(transcode) {}

  bool IsOpen() const override { return open_; }

  StatusOr<HttpResponse> Close() override {
    open_ = false;
    return HttpResponse{transcode_ ? 200 : 206, {}, {}};
  }

  StatusOr<ReadSourceResult> Read(char* buf, std::size_t n) override {
    auto limit = end_;
    if (fail_after_) {
      if (next_ - begin_ >= *fail_after_) {
        open_ = false;
        return TransientError();
      }
      limit = (std::min)(end_, begin_ + *fail_after_);
    }
    auto const count = (std::min)(static_cast<std::int64_t>(n), limit - next_);
    std::copy(contents_.begin() + next_, contents_.begin() + next_ + count,
              buf);
    next_ += count;
    auto const done = next_ == end_;
    auto const final_status = transcode_ ? 200 : 206;
    ReadSourceResult result{
        static_cast<std::size_t>(count),
        HttpResponse{done ? final_status : HttpStatusCode::kContinue, {}, {}}};
    if (first_read_) {
      auto const size = static_cast<std::int64_t>(contents_.size());
      result.generation = kGeneration;
      result.size = static_cast<std::uint64_t>(size);
      result.hashes = hashes_;
      result.response.headers.emplace("x-goog-generation",
                                      std::to_string(kGeneration));
      if (transcode_) {
        result.transformation = "gunzipped";
        result.response.headers.emplace(
            "x-guploader-response-body-transformations", "gunzipped");
      } else {
        result.response.headers.emplace(
            "content-range",
            absl::StrCat("bytes ", begin_, "-", end_ - 1, "/", size));
      }
    }
    first_read_ = false;
    if (done) open_ = false;
    return result;
  }

 private:
  std::string const& contents_;
  HashValues hashes_;
  std::int64_t begin_;
  std::int64_t end_;
  std::int64_t next_;
  std::optional<std::int64_t> fail_after_;
  bool transcode_;
  bool open_ = true;
  bool first_read_ = true;
};

/**
 * A fake service for `MockGenericStub::ReadObject()`.
 *
 * Records the requests, which may arrive from background threads when
 * windows are prefetched.
 */
class FakeObjectService {
 public:
  explicit FakeObjectService(std::int64_t size)
      : contents_(MakeContents(size)),
        hashes_{ComputeCrc32cChecksum(contents_), ComputeMD5Hash(contents_)} {}

  // The response to the request at @p index fails after returning @p n bytes.
  void set_fail_response_after(std::size_t index, std::int64_t n) {
    fail_after_[index] = n;
  }
  // Emulate decompressive transcoding: the service ignores the range of every
  // request, and returns the full object.
  void set_transcode(bool v) { transcode_ = v; }
  // Called (outside the lock) when a request arrives.
  void set_on_request(std::function<void(RecordedRequest const&)> f) {
    on_request_ = std::move(f);
  }

  auto Action() {
    return [this](rest_internal::RestContext&, Options const& options,
                  ReadObjectRangeRequest const& request) {
      return ReadObject(options, request);
    };
  }

  StatusOr<std::unique_ptr<ObjectReadSource>> ReadObject(
      Options const& options, ReadObjectRangeRequest const& request) {
    RecordedRequest recorded{
        request.RangeHeaderValue(),
        request.HasOption<Generation>()
            ? std::optional<std::int64_t>(
                  request.GetOption<Generation>().value())
            : std::nullopt,
        request.HasOption<IfMetagenerationMatch>()
            ? std::optional<std::int64_t>(
                  request.GetOption<IfMetagenerationMatch>().value())
            : std::nullopt,
        request.HasOption<IfMetagenerationNotMatch>()
            ? std::optional<std::int64_t>(
                  request.GetOption<IfMetagenerationNotMatch>().value())
            : std::nullopt,
        options.get<AdaptiveReadAttributionOption>(),
        std::this_thread::get_id()};
    if (on_request_) on_request_(recorded);
    std::optional<std::int64_t> fail_after;
    {
      std::lock_guard<std::mutex> lk(mu_);
      auto const f = fail_after_.find(requests_.size());
      if (f != fail_after_.end()) fail_after = f->second;
      requests_.push_back(std::move(recorded));
    }
    auto const size = static_cast<std::int64_t>(contents_.size());
    auto begin = request.StartingByte();
    auto end =
        request.HasOption<ReadRange>()
            ? (std::min)(request.GetOption<ReadRange>().value().end, size)
            : size;
    // Like GCS: a range that starts at the end of the object is not
    // satisfiable, a download without a range always succeeds.
    if (request.RequiresRangeHeader() && begin >= size) {
      return google::cloud::internal::OutOfRangeError(
          "Requested range not satisfiable", GCP_ERROR_INFO());
    }
    if (transcode_) {
      begin = 0;
      end = size;
    }
    return std::unique_ptr<ObjectReadSource>(std::make_unique<FakeRangeSource>(
        contents_, hashes_, begin, end, fail_after, transcode_));
  }

  std::string const& contents() const { return contents_; }
  HashValues const& hashes() const { return hashes_; }
  std::vector<RecordedRequest> requests() const {
    std::lock_guard<std::mutex> lk(mu_);
    return requests_;
  }

 private:
  std::string contents_;
  HashValues hashes_;
  std::map<std::size_t, std::int64_t> fail_after_;
  bool transcode_ = false;
  std::function<void(RecordedRequest const&)> on_request_;
  mutable std::mutex mu_;
  std::vector<RecordedRequest> requests_;
};

// Reads @p source until it closes, in reads of @p chunk bytes.
StatusOr<std::string> ReadAll(ObjectReadSource& source, std::size_t chunk) {
  std::string data;
  std::vector<char> buffer(chunk);
  while (source.IsOpen()) {
    auto r = source.Read(buffer.data(), buffer.size());
    if (!r) return std::move(r).status();
    data.append(buffer.data(), r->bytes_received);
  }
  return data;
}

std::shared_ptr<StorageConnectionImpl> MakeConnection(
    FakeObjectService& service) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options).Times(AtLeast(0));
  EXPECT_CALL(*mock, ReadObject).WillRepeatedly(service.Action());
  return StorageConnectionImpl::Create(std::move(mock));
}

TEST(StorageConnectionImplAdaptiveRead, DownloadsFullObjectInWindows) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(
          TestConfig()));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  auto data = ReadAll(**source, 300);
  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(*data == service.contents());

  // Every window is a bounded range, tagged, and pinned to the generation of
  // the first window.
  auto const requests = service.requests();
  EXPECT_THAT(requests,
              ElementsAre(RangeIs("bytes=0-1023"), RangeIs("bytes=1024-3071"),
                          RangeIs("bytes=3072-5999")));
  EXPECT_THAT(requests, Each(IsTagged(true)));
  EXPECT_THAT(requests, ElementsAre(GenerationIs(Eq(std::nullopt)),
                                    GenerationIs(Optional(kGeneration)),
                                    GenerationIs(Optional(kGeneration))));
}

TEST(StorageConnectionImplAdaptiveRead,
     MetagenerationPreconditionsClearedAfterFirstWindow) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(
          TestConfig()));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_option(IfMetagenerationMatch(7))
          .set_option(IfMetagenerationNotMatch(8)));
  ASSERT_THAT(source, IsOk());
  ASSERT_THAT(ReadAll(**source, 300), IsOk());

  auto const requests = service.requests();
  EXPECT_THAT(requests,
              ElementsAre(AllOf(IfMetagenerationMatchIs(Optional(7)),
                                IfMetagenerationNotMatchIs(Optional(8))),
                          AllOf(IfMetagenerationMatchIs(Eq(std::nullopt)),
                                IfMetagenerationNotMatchIs(Eq(std::nullopt)),
                                GenerationIs(Optional(kGeneration))),
                          AllOf(IfMetagenerationMatchIs(Eq(std::nullopt)),
                                IfMetagenerationNotMatchIs(Eq(std::nullopt)),
                                GenerationIs(Optional(kGeneration)))));
}

TEST(StorageConnectionImplAdaptiveRead,
     SeparatesEstimatorHistoryForOffsetReads) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  auto estimator = std::make_shared<AdaptiveReadSizeEstimator>();
  for (std::size_t i = 0; i != AdaptiveReadSizeEstimator::kMinSamples; ++i) {
    estimator->Record("test-bucket", 64 * 1024, /*reached_eof=*/false,
                      /*from_offset=*/false);
    estimator->Record("test-bucket", 1800 * 1024, /*reached_eof=*/false,
                      /*from_offset=*/true);
  }
  storage_experimental::AdaptiveReadRangeConfig config;
  config.prefetch_threshold = 0.0;
  google::cloud::internal::OptionsSpan const span(
      TestOptions()
          .set<storage_experimental::AdaptiveReadRangeOption>(config)
          .set<AdaptiveReadSizeEstimatorOption>(estimator));

  // A read from the start of the object uses the start-of-object history
  // (256 KiB), whereas a read from a positive offset uses the offset history
  // (2 MiB).
  ASSERT_THAT(connection->ReadObject(
                  ReadObjectRangeRequest("test-bucket", "test-object")),
              IsOk());
  ASSERT_THAT(connection->ReadObject(
                  ReadObjectRangeRequest("test-bucket", "test-object")
                      .set_option(ReadFromOffset(1000))),
              IsOk());
  EXPECT_THAT(service.requests(), ElementsAre(RangeIs("bytes=0-262143"),
                                              RangeIs("bytes=1000-2098151")));
}

TEST(StorageConnectionImplAdaptiveRead, DownloadsFromOffsetInWindows) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(
          TestConfig()));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_option(ReadFromOffset(2000)));
  ASSERT_THAT(source, IsOk());
  auto data = ReadAll(**source, 300);
  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(*data == service.contents().substr(2000));
  EXPECT_THAT(service.requests(), ElementsAre(RangeIs("bytes=2000-3023"),
                                              RangeIs("bytes=3024-5071"),
                                              RangeIs("bytes=5072-5999")));
}

TEST(StorageConnectionImplAdaptiveRead, DefaultConfiguration) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>({}));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  // The first window covers 1 MiB.
  EXPECT_THAT(service.requests(), ElementsAre(RangeIs("bytes=0-1048575")));
}

// An estimator whose history has small reads from `test-bucket`.
std::shared_ptr<AdaptiveReadSizeEstimator> SmallReadsEstimator() {
  auto estimator = std::make_shared<AdaptiveReadSizeEstimator>();
  for (int i = 0; i != 64; ++i) {
    estimator->Record("test-bucket", 400 * 1024, /*reached_eof=*/false);
  }
  return estimator;
}

TEST(StorageConnectionImplAdaptiveRead, FirstWindowUsesRecommendation) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  storage_experimental::AdaptiveReadRangeConfig config;
  config.prefetch_threshold = 0.0;
  google::cloud::internal::OptionsSpan const span(
      TestOptions()
          .set<storage_experimental::AdaptiveReadRangeOption>(config)
          .set<AdaptiveReadSizeEstimatorOption>(SmallReadsEstimator()));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  // The history recommends 512 KiB.
  EXPECT_THAT(service.requests(), ElementsAre(RangeIs("bytes=0-524287")));

  // Other buckets have no history.
  source = connection->ReadObject(
      ReadObjectRangeRequest("other-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  EXPECT_THAT(service.requests(), ElementsAre(RangeIs("bytes=0-524287"),
                                              RangeIs("bytes=0-1048575")));
}

TEST(StorageConnectionImplAdaptiveRead, LearningDisabledKeepsConfiguredWindow) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  storage_experimental::AdaptiveReadRangeConfig config;
  config.prefetch_threshold = 0.0;
  config.learn_initial_window = false;
  google::cloud::internal::OptionsSpan const span(
      TestOptions()
          .set<storage_experimental::AdaptiveReadRangeOption>(config)
          .set<AdaptiveReadSizeEstimatorOption>(SmallReadsEstimator()));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  EXPECT_THAT(service.requests(), ElementsAre(RangeIs("bytes=0-1048575")));
}

TEST(StorageConnectionImplAdaptiveRead, RecordsCompletedReads) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  auto estimator = std::make_shared<AdaptiveReadSizeEstimator>();
  google::cloud::internal::OptionsSpan const span(
      TestOptions()
          .set<storage_experimental::AdaptiveReadRangeOption>(TestConfig())
          .set<AdaptiveReadSizeEstimatorOption>(estimator));

  for (std::size_t i = 0; i != AdaptiveReadSizeEstimator::kMinSamples; ++i) {
    auto source = connection->ReadObject(
        ReadObjectRangeRequest("test-bucket", "test-object"));
    ASSERT_THAT(source, IsOk());
    ASSERT_THAT(ReadAll(**source, 4096), IsOk());
  }
  // Each read of the 6000 bytes reached the end of the object, the smallest
  // window is enough.
  auto const settings =
      MakeAdaptiveReadSettings(storage_experimental::AdaptiveReadRangeConfig{});
  EXPECT_THAT(estimator->Recommend("test-bucket", settings),
              Eq(AdaptiveReadSizeEstimator::kMinCandidate));
}

TEST(StorageConnectionImplAdaptiveRead, LearningDisabledDoesNotRecord) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  auto estimator = std::make_shared<AdaptiveReadSizeEstimator>();
  auto config = TestConfig();
  config.learn_initial_window = false;
  google::cloud::internal::OptionsSpan const span(
      TestOptions()
          .set<storage_experimental::AdaptiveReadRangeOption>(config)
          .set<AdaptiveReadSizeEstimatorOption>(estimator));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  ASSERT_THAT(ReadAll(**source, 4096), IsOk());
  EXPECT_THAT(estimator->bucket_count(), Eq(0U));
}

TEST(StorageConnectionImplAdaptiveRead, LogsSummaryWhenEnabled) {
  testing_util::ScopedLog log;
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions()
          .set<storage_experimental::AdaptiveReadRangeOption>(TestConfig())
          .set<LoggingComponentsOption>({"adaptive-read"}));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  ASSERT_THAT(ReadAll(**source, 4096), IsOk());
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(
          HasSubstr("adaptive read summary"), HasSubstr("bucket=test-bucket"),
          HasSubstr("close_reason=eof"), HasSubstr("bytes_delivered=6000"))));
}

TEST(StorageConnectionImplAdaptiveRead, NoSummaryLogByDefault) {
  testing_util::ScopedLog log;
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(
          TestConfig()));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  ASSERT_THAT(ReadAll(**source, 4096), IsOk());
  EXPECT_THAT(log.ExtractLines(),
              Not(Contains(HasSubstr("adaptive read summary"))));
}

TEST(StorageConnectionImplAdaptiveRead, DisabledByDefault) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(TestOptions());

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  auto data = ReadAll(**source, 300);
  ASSERT_THAT(data, IsOk());
  EXPECT_THAT(service.requests(),
              ElementsAre(AllOf(RangeIs(IsEmpty()), IsTagged(false))));
}

TEST(StorageConnectionImplAdaptiveRead, BoundedReadsAreUnchanged) {
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(
          TestConfig()));

  auto range = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_option(ReadRange(100, 5100)));
  ASSERT_THAT(range, IsOk());
  auto last = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_option(ReadLast(5000)));
  ASSERT_THAT(last, IsOk());

  EXPECT_THAT(service.requests(),
              ElementsAre(AllOf(RangeIs("bytes=100-5099"), IsTagged(false)),
                          AllOf(RangeIs("bytes=-5000"), IsTagged(false))));
}

TEST(StorageConnectionImplAdaptiveRead, EmptyObject) {
  // Any range of an empty object fails. The read falls back to a regular
  // download, which returns the (empty) object.
  FakeObjectService service(0);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(
          TestConfig()));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  auto data = ReadAll(**source, 300);
  ASSERT_THAT(data, IsOk());
  EXPECT_THAT(*data, IsEmpty());
  EXPECT_THAT(service.requests(),
              ElementsAre(AllOf(RangeIs("bytes=0-1023"), IsTagged(true)),
                          AllOf(RangeIs(IsEmpty()), IsTagged(false))));
}

TEST(StorageConnectionImplAdaptiveRead, OffsetPastEndOfObject) {
  // A regular download would fail too.
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(
          TestConfig()));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_option(ReadFromOffset(6000)));
  EXPECT_THAT(source, StatusIs(StatusCode::kOutOfRange));
  EXPECT_THAT(service.requests(), ElementsAre(RangeIs("bytes=6000-7023")));
}

TEST(StorageConnectionImplAdaptiveRead, ResumesInsideWindow) {
  // A failure in the middle of a window resumes at the current offset, without
  // going past the end of the window, and pinned to the same generation.
  FakeObjectService service(6000);
  service.set_fail_response_after(0, 300);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(
          TestConfig()));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  auto data = ReadAll(**source, 256);
  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(*data == service.contents());

  auto const requests = service.requests();
  EXPECT_THAT(
      requests,
      ElementsAre(
          AllOf(RangeIs("bytes=0-1023"), GenerationIs(Eq(std::nullopt))),
          AllOf(RangeIs("bytes=300-1023"), GenerationIs(Optional(kGeneration))),
          RangeIs("bytes=1024-3071"), RangeIs("bytes=3072-5999")));
  EXPECT_THAT(requests, Each(IsTagged(true)));
}

TEST(StorageConnectionImplAdaptiveRead,
     TranscodedObjectResumesAsRegularDownload) {
  // The service ignores the range of an object it decompresses on the fly. The
  // read switches to a regular download, which resumes like any other download:
  // from the current offset, without the range of the first window. Resuming
  // the first window instead would request `bytes=2000-1023`.
  FakeObjectService service(6000);
  service.set_transcode(true);
  // The regular download (the second request) fails past the first window.
  service.set_fail_response_after(1, 2000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(
          TestConfig()));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  auto data = ReadAll(**source, 300);
  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(*data == service.contents());
  EXPECT_THAT(service.requests(),
              ElementsAre(AllOf(RangeIs("bytes=0-1023"), IsTagged(true)),
                          AllOf(RangeIs(IsEmpty()), IsTagged(false)),
                          AllOf(RangeIs("bytes=2000-"), IsTagged(false))));
}

TEST(StorageConnectionImplAdaptiveRead, PrefetchesOnBackgroundThread) {
  FakeObjectService service(6000);
  std::promise<void> prefetched;
  service.set_on_request([&prefetched](RecordedRequest const& r) {
    if (r.range == "bytes=3072-5999") prefetched.set_value();
  });
  auto connection = MakeConnection(service);
  auto config = TestConfig();
  config.prefetch_threshold = 0.5;
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(config));

  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  // The first window is never prefetched past. Read past half of the second
  // window, [1024, 3072), and wait for the prefetch of the third window.
  std::vector<char> buffer(2100);
  auto r = (*source)->Read(buffer.data(), buffer.size());
  ASSERT_THAT(r, IsOk());
  std::string data(buffer.data(), r->bytes_received);
  ASSERT_THAT(prefetched.get_future().wait_for(std::chrono::seconds(30)),
              Eq(std::future_status::ready));

  auto rest = ReadAll(**source, 600);
  ASSERT_THAT(rest, IsOk());
  data += *rest;
  EXPECT_TRUE(data == service.contents());

  auto const requests = service.requests();
  ASSERT_THAT(requests, SizeIs(3));
  EXPECT_THAT(requests[0].thread, Eq(std::this_thread::get_id()));
  EXPECT_THAT(requests[1].thread, Eq(std::this_thread::get_id()));
  EXPECT_THAT(requests[2].thread, Ne(std::this_thread::get_id()));
  EXPECT_THAT(requests, Each(IsTagged(true)));
}

TEST(StorageConnectionImplAdaptiveRead, WindowsAreNotHedged) {
  FakeObjectService service(6000);
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options).Times(AtLeast(0));
  EXPECT_CALL(*mock, ReadObject).WillRepeatedly(service.Action());
  auto connection = StorageConnectionImpl::Create(
      std::move(mock),
      Options{}.set<storage_experimental::EnableReadHedgingOption>(true));
  google::cloud::internal::OptionsSpan const span(
      TestOptions()
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::AdaptiveReadRangeOption>(TestConfig()));

  // Windows are opened eagerly, on the calling thread.
  auto source = connection->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_THAT(source, IsOk());
  auto requests = service.requests();
  ASSERT_THAT(requests, SizeIs(1));
  EXPECT_THAT(requests[0].thread, Eq(std::this_thread::get_id()));

  auto data = ReadAll(**source, 300);
  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(*data == service.contents());
  EXPECT_THAT(service.requests(), Each(Field(&RecordedRequest::thread,
                                             Eq(std::this_thread::get_id()))));
}

TEST(StorageConnectionImplAdaptiveRead, ValidatesChecksumsEndToEnd) {
  // Use an object with checksums, served in windows by the fake service, and
  // validated by `ObjectReadStreambuf` like `Client::ReadObject()` does.
  FakeObjectService service(6000);
  auto connection = MakeConnection(service);
  google::cloud::internal::OptionsSpan const span(
      TestOptions().set<storage_experimental::AdaptiveReadRangeOption>(
          TestConfig()));

  auto const request = ReadObjectRangeRequest("test-bucket", "test-object");
  auto source = connection->ReadObject(request);
  ASSERT_THAT(source, IsOk());
  ObjectReadStreambuf buf(request, *std::move(source));
  std::istream stream(&buf);
  std::string data{std::istreambuf_iterator<char>(stream), {}};
  EXPECT_TRUE(data == service.contents());
  EXPECT_THAT(buf.status(), IsOk());
  EXPECT_THAT(buf.received_hash(), HasSubstr(service.hashes().crc32c));
  EXPECT_THAT(buf.computed_hash(), Eq(buf.received_hash()));
  EXPECT_THAT(buf.size(), Optional(Eq(std::uint64_t{6000})));
  EXPECT_THAT(buf.generation(), Optional(kGeneration));
  EXPECT_THAT(buf.headers().count("content-range"), Eq(0U));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
