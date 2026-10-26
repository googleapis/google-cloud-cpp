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

#include "google/cloud/storage/internal/adaptive_object_read_source.h"
#include "google/cloud/storage/hash_mismatch_error.h"
#include "google/cloud/storage/hashing_options.h"
#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/storage/internal/hedging_thread_pool.h"
#include "google/cloud/storage/internal/object_read_streambuf.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_cat.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <opentelemetry/trace/scope.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <limits>
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

using ::google::cloud::storage::testing::MockObjectReadSource;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SizeIs;

using Window = AdaptiveObjectReadSource::Window;
using WindowFactory = AdaptiveObjectReadSource::WindowFactory;
using Executor = AdaptiveObjectReadSource::Executor;
using Headers = std::multimap<std::string, std::string>;

std::int64_t constexpr kObjectSize = 20000;
std::int64_t constexpr kGeneration = 1234;

MATCHER_P2(WindowIs, offset, length, "") {
  return ExplainMatchResult(AllOf(Field(&Window::offset, Eq(offset)),
                                  Field(&Window::length, Eq(length))),
                            arg, result_listener);
}

MATCHER_P(WindowGeneration, matcher, "") {
  return ExplainMatchResult(matcher, arg.generation, result_listener);
}

// Match a `std::shared_ptr<WindowStats>`.
MATCHER_P(WindowCompleted, expected, "") {
  return ExplainMatchResult(Eq(expected), arg->completed.load(),
                            result_listener);
}

MATCHER_P(WindowDestroyed, expected, "") {
  return ExplainMatchResult(Eq(expected), arg->destroyed.load(),
                            result_listener);
}

// Small windows keep the test objects small. Prefetching is disabled, the
// tests for prefetching enable it explicitly.
AdaptiveReadSettings TestSettings() {
  AdaptiveReadSettings settings{/*initial_window=*/1024, /*max_window=*/8192,
                                /*growth_factor=*/2.0,
                                /*prefetch_threshold=*/0.0};
  settings.max_drain = 1024;
  return settings;
}

AdaptiveReadSettings PrefetchSettings() {
  auto settings = TestSettings();
  settings.prefetch_threshold = 0.5;
  return settings;
}

std::string MakeContents(std::int64_t size) {
  std::string contents(static_cast<std::size_t>(size), '\0');
  std::uint32_t state = 42;
  std::generate(contents.begin(), contents.end(), [&state] {
    state = state * 1664525U + 1013904223U;
    return static_cast<char>(state >> 24);
  });
  return contents;
}

/// How the fake service responds to a range request.
enum class Behavior {
  // Honor the range, as GCS does.
  kHonorRange,
  // Ignore the range and return the full object, as GCS does when it
  // decompresses (transcodes) an object on the fly.
  kTranscode,
  // Ignore the range, without reporting a transformation.
  kIgnoreRange,
  // Honor the range in the first window only.
  kIgnoreRangeAfterFirst,
};

/// The observable state of a single window. Prefetching opens windows from
/// other threads, hence the atomics.
struct WindowStats {
  std::atomic<std::int64_t> delivered{0};
  // The response was read to the end, so the connection can be reused.
  std::atomic<bool> completed{false};
  std::atomic<bool> closed{false};
  std::atomic<bool> destroyed{false};
};

/**
 * Serves one window of a fake object, emulating the sources created by the
 * REST and gRPC stubs.
 *
 * Over REST the headers arrive with the first read, and the response to a
 * range request includes `content-range`. Over gRPC the metadata (generation,
 * size, hashes) arrives with the first read, and the headers (trailers) with
 * the last read.
 *
 * Like libcurl, the source is still open after it returns the last byte. The
 * next read returns no data, and completes the response.
 */
class FakeWindowSource : public ObjectReadSource {
 public:
  struct Config {
    HashValues hashes;
    std::int64_t generation;
    bool grpc;
    bool report_size;
    bool ignore_range;
    bool transcode;
  };

  FakeWindowSource(std::string const& contents, Config config,
                   Window const& window, std::int64_t begin, std::int64_t end,
                   std::shared_ptr<WindowStats> stats)
      : contents_(contents),
        config_(std::move(config)),
        window_(window),
        begin_(begin),
        end_(end),
        next_(begin),
        stats_(std::move(stats)) {}

  ~FakeWindowSource() override { stats_->destroyed = true; }

  bool IsOpen() const override { return open_; }

  StatusOr<HttpResponse> Close() override {
    open_ = false;
    stats_->closed = true;
    return HttpResponse{config_.grpc ? HttpStatusCode::kOk : 206, {}, {}};
  }

  StatusOr<ReadSourceResult> Read(char* buf, std::size_t n) override {
    if (!open_) {
      return google::cloud::internal::FailedPreconditionError("closed");
    }
    auto const count = (std::min)(static_cast<std::int64_t>(n), end_ - next_);
    std::copy(contents_.begin() + next_, contents_.begin() + next_ + count,
              buf);
    next_ += count;
    stats_->delivered += count;
    ReadSourceResult result{static_cast<std::size_t>(count),
                            HttpResponse{HttpStatusCode::kContinue, {}, {}}};
    if (first_read_) AddMetadata(result);
    first_read_ = false;
    // A gRPC stream finds its end (and returns its trailers) in the read that
    // asks for more data than the stream has left, which may also return
    // data. libcurl reports the end of a response in the read after the last
    // byte.
    auto const end_of_stream =
        config_.grpc ? count < static_cast<std::int64_t>(n) : count == 0;
    if (end_of_stream) {
      open_ = false;
      stats_->completed = true;
      if (config_.grpc) {
        result.response.headers.emplace(
            "x-test-trailer", absl::StrCat("window-", window_.offset));
      } else {
        result.response.status_code = config_.ignore_range ? 200 : 206;
      }
    }
    return result;
  }

 private:
  void AddMetadata(ReadSourceResult& result) const {
    auto const size = static_cast<std::int64_t>(contents_.size());
    result.generation = config_.generation;
    result.hashes = config_.hashes;
    if (config_.report_size) result.size = static_cast<std::uint64_t>(size);
    if (config_.grpc) return;
    auto& headers = result.response.headers;
    headers.emplace("x-goog-generation", std::to_string(config_.generation));
    headers.emplace("x-goog-hash", "crc32c=" + config_.hashes.crc32c);
    headers.emplace("x-goog-hash", "md5=" + config_.hashes.md5);
    headers.emplace("content-length", std::to_string(end_ - begin_));
    if (config_.transcode) {
      headers.emplace("x-guploader-response-body-transformations", "gunzipped");
      result.transformation = "gunzipped";
    }
    if (config_.ignore_range) return;
    headers.emplace("content-range",
                    absl::StrCat("bytes ", begin_, "-", end_ - 1, "/", size));
  }

  std::string const& contents_;
  Config config_;
  Window window_;
  std::int64_t begin_;
  std::int64_t end_;
  std::int64_t next_;
  bool open_ = true;
  bool first_read_ = true;
  std::shared_ptr<WindowStats> stats_;
};

/**
 * A fake service: creates the windows of an adaptive read and records them.
 *
 * Windows may be opened from a background thread (prefetching), so the
 * records are protected by a mutex.
 */
class FakeService {
 public:
  explicit FakeService(std::int64_t size)
      : contents_(MakeContents(size)),
        hashes_{ComputeCrc32cChecksum(contents_), ComputeMD5Hash(contents_)} {}

  FakeService& set_grpc(bool v) {
    grpc_ = v;
    return *this;
  }
  FakeService& set_report_size(bool v) {
    report_size_ = v;
    return *this;
  }
  FakeService& set_behavior(Behavior v) {
    behavior_ = v;
    return *this;
  }
  FakeService& set_generation(std::int64_t v) {
    generation_ = v;
    return *this;
  }
  FakeService& set_hashes(HashValues v) {
    hashes_ = std::move(v);
    return *this;
  }
  // Serves corrupted data for the byte at @p offset.
  FakeService& set_corrupt_offset(std::int64_t offset) {
    corrupted_ = contents_;
    corrupted_[static_cast<std::size_t>(offset)] ^= 0x01;
    return *this;
  }
  // Fails to open the window starting at @p offset.
  FakeService& set_open_error(std::int64_t offset, Status status) {
    open_errors_[offset] = std::move(status);
    return *this;
  }
  // The window starting at @p offset ends after @p length bytes.
  FakeService& set_short_window(std::int64_t offset, std::int64_t length) {
    short_windows_[offset] = length;
    return *this;
  }
  // Called (outside the lock) before each window is created.
  FakeService& set_on_open(std::function<void(Window const&)> f) {
    on_open_ = std::move(f);
    return *this;
  }

  std::shared_ptr<WindowFactory const> Factory() {
    return std::make_shared<WindowFactory const>(
        [this](Window const& w) { return Open(w); });
  }

  StatusOr<std::unique_ptr<ObjectReadSource>> Open(Window const& window) {
    if (on_open_) on_open_(window);
    auto const size = static_cast<std::int64_t>(contents_.size());
    auto stats = std::make_shared<WindowStats>();
    std::lock_guard<std::mutex> lk(mu_);
    auto const index = windows_.size();
    windows_.push_back(window);
    stats_.push_back(stats);
    previous_completed_.push_back(index == 0 ||
                                  stats_[index - 1]->completed.load());
    auto const error = open_errors_.find(window.offset);
    if (error != open_errors_.end()) return error->second;
    // GCS rejects ranges that start at (or past) the end of the object.
    if (window.offset >= size) {
      return google::cloud::internal::OutOfRangeError("416", GCP_ERROR_INFO());
    }
    auto const ignore =
        behavior_ == Behavior::kTranscode ||
        behavior_ == Behavior::kIgnoreRange ||
        (behavior_ == Behavior::kIgnoreRangeAfterFirst && index != 0);
    auto begin = ignore ? 0 : window.offset;
    auto end = ignore ? size : (std::min)(window.offset + window.length, size);
    auto const s = short_windows_.find(window.offset);
    if (s != short_windows_.end()) end = begin + s->second;
    auto const& contents = corrupted_.empty() ? contents_ : corrupted_;
    FakeWindowSource::Config config{
        hashes_,
        generation_,
        grpc_,
        report_size_,
        /*ignore_range=*/ignore,
        /*transcode=*/behavior_ == Behavior::kTranscode};
    return std::unique_ptr<ObjectReadSource>(std::make_unique<FakeWindowSource>(
        contents, std::move(config), window, begin, end, std::move(stats)));
  }

  // Fails the regular downloads (see `Unbounded()`) with @p status.
  FakeService& set_unbounded_error(Status status) {
    unbounded_error_ = std::move(status);
    return *this;
  }

  // Creates regular downloads from @p offset, as the `UnboundedFactory` of an
  // adaptive read does.
  AdaptiveObjectReadSource::UnboundedFactory Unbounded(std::int64_t offset) {
    return [this, offset] { return OpenUnbounded(offset); };
  }

  StatusOr<std::unique_ptr<ObjectReadSource>> OpenUnbounded(
      std::int64_t offset) {
    auto const size = static_cast<std::int64_t>(contents_.size());
    auto stats = std::make_shared<WindowStats>();
    std::lock_guard<std::mutex> lk(mu_);
    unbounded_offsets_.push_back(offset);
    unbounded_stats_.push_back(stats);
    if (!unbounded_error_.ok()) return unbounded_error_;
    // Without transformations the service honors `Range: bytes=offset-`, a
    // download from offset 0 has no range. A transformed object is always
    // returned in full.
    auto const transformed = behavior_ == Behavior::kTranscode ||
                             behavior_ == Behavior::kIgnoreRange;
    auto const begin = transformed ? 0 : offset;
    FakeWindowSource::Config config{
        hashes_,
        generation_,
        grpc_,
        report_size_,
        /*ignore_range=*/transformed || offset == 0,
        /*transcode=*/behavior_ == Behavior::kTranscode};
    return std::unique_ptr<ObjectReadSource>(std::make_unique<FakeWindowSource>(
        contents_, std::move(config), Window{offset, size - offset, {}}, begin,
        size, std::move(stats)));
  }

  std::string const& contents() const { return contents_; }
  HashValues const& hashes() const { return hashes_; }

  std::vector<Window> windows() const {
    std::lock_guard<std::mutex> lk(mu_);
    return windows_;
  }
  std::vector<std::shared_ptr<WindowStats>> stats() const {
    std::lock_guard<std::mutex> lk(mu_);
    return stats_;
  }
  // For each window, whether the previous window was completed when it
  // opened. A completed response returns its connection to the pool.
  std::vector<bool> previous_completed() const {
    std::lock_guard<std::mutex> lk(mu_);
    return previous_completed_;
  }
  // The start offset of each regular download.
  std::vector<std::int64_t> unbounded_offsets() const {
    std::lock_guard<std::mutex> lk(mu_);
    return unbounded_offsets_;
  }
  std::vector<std::shared_ptr<WindowStats>> unbounded_stats() const {
    std::lock_guard<std::mutex> lk(mu_);
    return unbounded_stats_;
  }

 private:
  std::string contents_;
  std::string corrupted_;
  HashValues hashes_;
  std::int64_t generation_ = kGeneration;
  bool grpc_ = false;
  bool report_size_ = true;
  Behavior behavior_ = Behavior::kHonorRange;
  std::map<std::int64_t, Status> open_errors_;
  std::map<std::int64_t, std::int64_t> short_windows_;
  std::function<void(Window const&)> on_open_;
  Status unbounded_error_;

  mutable std::mutex mu_;
  std::vector<Window> windows_;
  std::vector<std::shared_ptr<WindowStats>> stats_;
  std::vector<bool> previous_completed_;
  std::vector<std::int64_t> unbounded_offsets_;
  std::vector<std::shared_ptr<WindowStats>> unbounded_stats_;
};

/// Holds prefetch tasks until the test runs them.
class ManualExecutor {
 public:
  Executor executor() {
    return [this](std::function<void()> task) {
      tasks_.push_back(std::move(task));
      return true;
    };
  }
  std::size_t pending() const { return tasks_.size(); }
  void RunAll() {
    auto tasks = std::move(tasks_);
    tasks_.clear();
    for (auto& t : tasks) t();
  }

 private:
  std::vector<std::function<void()>> tasks_;
};

std::unique_ptr<ObjectReadSource> MakeSource(
    FakeService& service, AdaptiveReadSettings settings, std::int64_t offset,
    Executor executor,
    AdaptiveObjectReadSource::SummaryCallback on_summary = {}) {
  auto source = MakeAdaptiveObjectReadSource(
      service.Factory(), service.Unbounded(offset), std::move(executor),
      settings, offset, /*generation=*/std::nullopt, std::move(on_summary));
  if (!source) return nullptr;
  return *std::move(source);
}

/// Records the summaries reported by a source.
class SummaryRecorder {
 public:
  AdaptiveObjectReadSource::SummaryCallback callback() {
    return [this](AdaptiveReadSummary const& s) { summaries_.push_back(s); };
  }
  std::vector<AdaptiveReadSummary> const& summaries() const {
    return summaries_;
  }

 private:
  std::vector<AdaptiveReadSummary> summaries_;
};

struct ReadAllResult {
  std::string data;
  std::vector<ReadSourceResult> results;
  Status status;
};

// Reads @p source until it closes, in reads of @p chunk bytes.
ReadAllResult ReadAll(ObjectReadSource& source, std::size_t chunk) {
  ReadAllResult all;
  std::vector<char> buffer(chunk);
  while (source.IsOpen()) {
    auto r = source.Read(buffer.data(), buffer.size());
    if (!r) {
      all.status = std::move(r).status();
      break;
    }
    all.data.append(buffer.data(), r->bytes_received);
    all.results.push_back(*std::move(r));
  }
  return all;
}

// The headers an application sees: `ObjectReadStreambuf` accumulates the
// headers of all the results.
Headers AllHeaders(std::vector<ReadSourceResult> const& results) {
  Headers headers;
  for (auto const& r : results) {
    headers.insert(r.response.headers.begin(), r.response.headers.end());
  }
  return headers;
}

TEST(AdaptiveReadSettings, Defaults) {
  auto const settings =
      MakeAdaptiveReadSettings(storage_experimental::AdaptiveReadRangeConfig{});
  EXPECT_THAT(settings.initial_window, Eq(1024 * 1024));
  EXPECT_THAT(settings.max_window, Eq(64 * 1024 * 1024));
  EXPECT_THAT(settings.growth_factor, Eq(2.0));
  EXPECT_THAT(settings.prefetch_threshold, Eq(0.5));
  EXPECT_THAT(settings.max_drain, Eq(1024 * 1024));
  EXPECT_THAT(settings.request_cost_bytes, Eq(1024 * 1024));
  EXPECT_TRUE(settings.learn_initial_window);
}

TEST(AdaptiveReadSettings, CopiesValidValues) {
  storage_experimental::AdaptiveReadRangeConfig config;
  config.max_drain = 0;
  config.request_cost_bytes = 0;
  config.learn_initial_window = false;
  auto settings = MakeAdaptiveReadSettings(config);
  EXPECT_THAT(settings.max_drain, Eq(0));
  EXPECT_THAT(settings.request_cost_bytes, Eq(0));
  EXPECT_FALSE(settings.learn_initial_window);

  config.max_drain = 4096;
  config.request_cost_bytes = 65536;
  settings = MakeAdaptiveReadSettings(config);
  EXPECT_THAT(settings.max_drain, Eq(4096));
  EXPECT_THAT(settings.request_cost_bytes, Eq(65536));
}

TEST(AdaptiveReadSettings, ReplacesInvalidValues) {
  storage_experimental::AdaptiveReadRangeConfig config;
  config.initial_window = 0;
  config.max_window = 16;
  config.growth_factor = 0.5;
  config.prefetch_threshold = 0.25;
  config.max_drain = -1;
  config.request_cost_bytes = -1;
  auto settings = MakeAdaptiveReadSettings(config);
  EXPECT_THAT(settings.initial_window, Eq(1024 * 1024));
  EXPECT_THAT(settings.max_window, Eq(1024 * 1024));
  EXPECT_THAT(settings.growth_factor, Eq(1.0));
  EXPECT_THAT(settings.prefetch_threshold, Eq(0.25));
  EXPECT_THAT(settings.max_drain, Eq(1024 * 1024));
  EXPECT_THAT(settings.request_cost_bytes, Eq(1024 * 1024));

  config.initial_window = -5;
  config.growth_factor = std::numeric_limits<double>::quiet_NaN();
  settings = MakeAdaptiveReadSettings(config);
  EXPECT_THAT(settings.initial_window, Eq(1024 * 1024));
  EXPECT_THAT(settings.growth_factor, Eq(1.0));
}

TEST(AdaptiveReadSettings, DefaultWindowSizes) {
  auto const settings =
      MakeAdaptiveReadSettings(storage_experimental::AdaptiveReadRangeConfig{});
  std::vector<std::int64_t> sizes{settings.initial_window};
  for (int i = 0; i != 7; ++i) {
    sizes.push_back(NextAdaptiveWindowSize(sizes.back(), settings));
  }
  std::int64_t constexpr kMiB = 1024 * 1024;
  EXPECT_THAT(sizes, ElementsAre(1 * kMiB, 2 * kMiB, 4 * kMiB, 8 * kMiB,
                                 16 * kMiB, 32 * kMiB, 64 * kMiB, 64 * kMiB));
}

TEST(AdaptiveReadSettings, FractionalGrowth) {
  AdaptiveReadSettings const settings{1000, 4000, 1.5, 0.0};
  std::vector<std::int64_t> sizes{settings.initial_window};
  for (int i = 0; i != 5; ++i) {
    sizes.push_back(NextAdaptiveWindowSize(sizes.back(), settings));
  }
  EXPECT_THAT(sizes, ElementsAre(1000, 1500, 2250, 3375, 4000, 4000));
}

TEST(AdaptiveReadSettings, NoAndInfiniteGrowth) {
  AdaptiveReadSettings settings{1000, 4000, 1.0, 0.0};
  EXPECT_THAT(NextAdaptiveWindowSize(1000, settings), Eq(1000));
  settings.growth_factor = std::numeric_limits<double>::infinity();
  EXPECT_THAT(NextAdaptiveWindowSize(1000, settings), Eq(4000));
}

TEST(AdaptiveReadSettings, RequestsPerGiB) {
  // The cost of reading a full object: a 1 GiB object takes 22 requests with
  // the default configuration, well within the budget of 25.
  auto const settings =
      MakeAdaptiveReadSettings(storage_experimental::AdaptiveReadRangeConfig{});
  std::int64_t constexpr kGiB = 1024 * 1024 * 1024;
  int requests = 0;
  auto window = settings.initial_window;
  for (std::int64_t offset = 0; offset < kGiB; offset += window) {
    if (requests != 0) window = NextAdaptiveWindowSize(window, settings);
    ++requests;
  }
  EXPECT_THAT(requests, Eq(22));
}

TEST(AdaptiveReadApiClientHeader, Untagged) {
  EXPECT_THAT(AdaptiveReadApiClientHeader(Options{}), Eq(x_goog_api_client()));
  EXPECT_THAT(AdaptiveReadApiClientHeader(
                  Options{}.set<AdaptiveReadAttributionOption>(false)),
              Eq(x_goog_api_client()));
}

TEST(AdaptiveReadApiClientHeader, Tagged) {
  EXPECT_THAT(AdaptiveReadApiClientHeader(
                  Options{}.set<AdaptiveReadAttributionOption>(true)),
              Eq(x_goog_api_client() + " gccl-feat/adaptive-read"));
}

TEST(AdaptiveObjectReadSource, ReadsFullObjectInGrowingWindows) {
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 512);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  EXPECT_FALSE(source->IsOpen());
  // The last window is clamped to the end of the object.
  EXPECT_THAT(
      service.windows(),
      ElementsAre(WindowIs(0, 1024), WindowIs(1024, 2048), WindowIs(3072, 4096),
                  WindowIs(7168, 8192), WindowIs(15360, 4640)));
}

TEST(AdaptiveObjectReadSource, ReadsFromOffset) {
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 5000, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 512);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents().substr(5000));
  EXPECT_THAT(service.windows(),
              ElementsAre(WindowIs(5000, 1024), WindowIs(6024, 2048),
                          WindowIs(8072, 4096), WindowIs(12168, 7832)));
}

TEST(AdaptiveObjectReadSource, SmallObjectInFirstWindow) {
  FakeService service(100);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 512);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  EXPECT_THAT(service.windows(), ElementsAre(WindowIs(0, 1024)));
  EXPECT_THAT(actual.results, SizeIs(1));
}

TEST(AdaptiveObjectReadSource, FillsBufferAcrossWindows) {
  // `std::istream::read()` treats a short read as the end of the stream, so a
  // read that spans several windows must still return all the bytes.
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(10000);
  auto r = source->Read(buffer.data(), buffer.size());
  ASSERT_THAT(r, IsOk());
  EXPECT_THAT(r->bytes_received, Eq(10000U));
  EXPECT_THAT(r->response.status_code, Eq(HttpStatusCode::kContinue));
  EXPECT_TRUE(source->IsOpen());
  EXPECT_TRUE(std::string(buffer.begin(), buffer.end()) ==
              service.contents().substr(0, 10000));

  // A read past the end of the object returns the remaining bytes, and closes
  // the stream.
  buffer.resize(2 * kObjectSize);
  r = source->Read(buffer.data(), buffer.size());
  ASSERT_THAT(r, IsOk());
  EXPECT_THAT(r->bytes_received,
              Eq(static_cast<std::size_t>(kObjectSize - 10000)));
  EXPECT_THAT(r->response.status_code, Eq(206));
  EXPECT_FALSE(source->IsOpen());
}

TEST(AdaptiveObjectReadSource, PinsGenerationOfFirstWindow) {
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  EXPECT_THAT(ReadAll(*source, 4096).status, IsOk());

  auto const windows = service.windows();
  ASSERT_THAT(windows, SizeIs(5));
  EXPECT_THAT(windows.front(), WindowGeneration(Eq(std::nullopt)));
  EXPECT_THAT(std::vector<Window>(windows.begin() + 1, windows.end()),
              Each(WindowGeneration(Optional(kGeneration))));
}

TEST(AdaptiveObjectReadSource, KeepsRequestedGeneration) {
  FakeService service(kObjectSize);
  service.set_generation(42);
  auto source = MakeAdaptiveObjectReadSource(service.Factory(), {}, Executor{},
                                             TestSettings(), 0, 42);
  ASSERT_THAT(source, IsOk());
  EXPECT_THAT(ReadAll(**source, 4096).status, IsOk());
  EXPECT_THAT(service.windows(), Each(WindowGeneration(Optional(42))));
}

TEST(AdaptiveObjectReadSource, RewritesHeadersForFullObject) {
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  auto const actual = ReadAll(*source, 512);
  ASSERT_THAT(actual.status, IsOk());

  // A download of the full object has no `content-range`, and its
  // `content-length` is the size of the object. The headers of later windows
  // are not repeated.
  auto const headers = AllHeaders(actual.results);
  EXPECT_THAT(headers, Contains(Pair("content-length", "20000")).Times(1));
  EXPECT_THAT(headers, Not(Contains(Pair("content-range", _))));
  EXPECT_THAT(headers, Contains(Pair("x-goog-generation", "1234")).Times(1));
  EXPECT_THAT(headers, Contains(Pair("x-goog-hash", _)).Times(2));

  ASSERT_THAT(actual.results, Not(IsEmpty()));
  auto const& first = actual.results.front();
  EXPECT_THAT(first.size, Optional(Eq(std::uint64_t{kObjectSize})));
  EXPECT_THAT(first.generation, Optional(kGeneration));
  EXPECT_THAT(first.hashes.crc32c, Eq(service.hashes().crc32c));
  EXPECT_THAT(first.hashes.md5, Eq(service.hashes().md5));
}

TEST(AdaptiveObjectReadSource, RewritesHeadersForOffset) {
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 5000, Executor{});
  ASSERT_THAT(source, NotNull());
  auto const actual = ReadAll(*source, 512);
  ASSERT_THAT(actual.status, IsOk());

  auto const headers = AllHeaders(actual.results);
  EXPECT_THAT(headers, Contains(Pair("content-length", "15000")).Times(1));
  EXPECT_THAT(
      headers,
      Contains(Pair("content-range", "bytes 5000-19999/20000")).Times(1));
}

TEST(AdaptiveObjectReadSource, CompletesEachWindowBeforeTheNext) {
  // Each window is read to the end of its response before the next window is
  // opened, so the next request can reuse the connection.
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  EXPECT_THAT(ReadAll(*source, 1000).status, IsOk());

  EXPECT_THAT(service.previous_completed(), Each(true));
  EXPECT_THAT(service.stats(),
              Each(AllOf(WindowCompleted(true), WindowDestroyed(true))));
}

TEST(AdaptiveObjectReadSource, KeepsTrailersOfFirstGrpcWindow) {
  FakeService service(kObjectSize);
  service.set_grpc(true);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  auto const actual = ReadAll(*source, 512);
  ASSERT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());

  // A gRPC response returns its headers with the last read, which the source
  // issues when it completes the window.
  auto const headers = AllHeaders(actual.results);
  EXPECT_THAT(headers, ElementsAre(Pair("x-test-trailer", "window-0")));
}

TEST(AdaptiveObjectReadSource, GrpcObjectSmallerThanFirstWindow) {
  // The read that finds the end of a gRPC stream returns its headers together
  // with the last data. Without `content-range`, those headers resemble a REST
  // response that ignored the range, but gRPC always honors it.
  FakeService service(100);
  service.set_grpc(true);
  auto open_unbounded = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ADD_FAILURE() << "unexpected call";
    return google::cloud::internal::UnknownError("unexpected");
  };
  auto source =
      MakeAdaptiveObjectReadSource(service.Factory(), open_unbounded,
                                   Executor{}, TestSettings(), 0, std::nullopt);
  ASSERT_THAT(source, IsOk());

  auto const actual = ReadAll(**source, 512);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  EXPECT_THAT(actual.results, SizeIs(1));
  EXPECT_THAT(service.windows(), ElementsAre(WindowIs(0, 1024)));
  EXPECT_THAT(AllHeaders(actual.results),
              ElementsAre(Pair("x-test-trailer", "window-0")));
}

TEST(AdaptiveObjectReadSource, TranscodedObjectFallsBackToRegularDownload) {
  FakeService service(kObjectSize);
  service.set_behavior(Behavior::kTranscode);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 512);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  // The first window shows that the service ignored the range. Its response
  // is discarded, and a regular download returns the data.
  EXPECT_THAT(service.windows(), ElementsAre(WindowIs(0, 1024)));
  EXPECT_THAT(service.stats(), ElementsAre(AllOf(WindowCompleted(false),
                                                 WindowDestroyed(true))));
  EXPECT_THAT(service.unbounded_offsets(), ElementsAre(0));
  EXPECT_THAT(service.unbounded_stats(), ElementsAre(WindowCompleted(true)));
  ASSERT_THAT(actual.results, Not(IsEmpty()));
  EXPECT_THAT(actual.results.front().transformation, Optional(Eq("gunzipped")));
  // The headers are those of the regular download, without changes.
  EXPECT_THAT(AllHeaders(actual.results),
              Contains(Pair("content-length", "20000")).Times(1));
}

TEST(AdaptiveObjectReadSource, TranscodedObjectFromOffset) {
  // The service returns the full object for a transcoded download from an
  // offset. Adaptive reads return the same data as a regular download.
  FakeService service(kObjectSize);
  service.set_behavior(Behavior::kTranscode);
  auto source = MakeSource(service, TestSettings(), 5000, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 4096);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  EXPECT_THAT(service.windows(), ElementsAre(WindowIs(5000, 1024)));
  EXPECT_THAT(service.unbounded_offsets(), ElementsAre(5000));
}

TEST(AdaptiveObjectReadSource, TranscodedObjectFillsLargeReads) {
  FakeService service(kObjectSize);
  service.set_behavior(Behavior::kTranscode);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  // The first read is larger than the first window.
  std::vector<char> buffer(8000);
  auto r = source->Read(buffer.data(), buffer.size());
  ASSERT_THAT(r, IsOk());
  EXPECT_THAT(r->bytes_received, Eq(8000U));
  EXPECT_TRUE(source->IsOpen());
  EXPECT_TRUE(std::string(buffer.begin(), buffer.end()) ==
              service.contents().substr(0, 8000));
}

TEST(AdaptiveObjectReadSource, IgnoredRangeInFirstWindowFallsBack) {
  // A REST response without `content-range` did not honor the range, even
  // without a transformation.
  FakeService service(kObjectSize);
  service.set_behavior(Behavior::kIgnoreRange);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 512);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  EXPECT_THAT(service.windows(), ElementsAre(WindowIs(0, 1024)));
  EXPECT_THAT(service.unbounded_offsets(), ElementsAre(0));
}

TEST(AdaptiveObjectReadSource, FallbackErrorIsReported) {
  FakeService service(kObjectSize);
  service.set_behavior(Behavior::kTranscode)
      .set_unbounded_error(PermanentError());
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(512);
  EXPECT_THAT(source->Read(buffer.data(), buffer.size()),
              StatusIs(PermanentError().code()));
  EXPECT_FALSE(source->IsOpen());
  EXPECT_THAT(service.stats(), Each(WindowDestroyed(true)));
}

TEST(AdaptiveObjectReadSource, FallbackWithoutUnboundedFactoryIsAnError) {
  FakeService service(kObjectSize);
  service.set_behavior(Behavior::kTranscode);
  auto source = MakeAdaptiveObjectReadSource(
      service.Factory(), AdaptiveObjectReadSource::UnboundedFactory{},
      Executor{}, TestSettings(), 0, std::nullopt);
  ASSERT_THAT(source, IsOk());

  std::vector<char> buffer(512);
  EXPECT_THAT((*source)->Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kFailedPrecondition,
                       HasSubstr("did not honor the range")));
  EXPECT_FALSE((*source)->IsOpen());
}

TEST(AdaptiveObjectReadSource, IgnoredRangeInLaterWindowIsAnError) {
  // Returning data from the wrong offset would corrupt the download.
  FakeService service(kObjectSize);
  service.set_behavior(Behavior::kIgnoreRangeAfterFirst);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 4096);
  EXPECT_THAT(actual.status, StatusIs(StatusCode::kFailedPrecondition,
                                      HasSubstr("did not honor the range")));
  EXPECT_TRUE(actual.data == service.contents().substr(0, 1024));
  EXPECT_FALSE(source->IsOpen());
}

TEST(AdaptiveObjectReadSource, NoPrefetchInFirstWindow) {
  // Many applications read a small prefix and close the stream, a prefetch
  // from the first window would open a window they never use.
  FakeService service(kObjectSize);
  ManualExecutor executor;
  SummaryRecorder recorder;
  auto source = MakeSource(service, PrefetchSettings(), 0, executor.executor(),
                           recorder.callback());
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(256);
  // Read the first window in full, well past the threshold.
  for (int i = 0; i != 4; ++i) {
    ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
    EXPECT_THAT(executor.pending(), Eq(0U));
  }
  EXPECT_THAT(service.windows(), ElementsAre(WindowIs(0, 1024)));
  EXPECT_THAT(source->Close(), IsOk());
  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  EXPECT_THAT(recorder.summaries()[0].prefetch_started, Eq(0));
  EXPECT_THAT(recorder.summaries()[0].windows_opened, Eq(1));
}

TEST(AdaptiveObjectReadSource, NoPrefetchInFirstWindowWithLargeRead) {
  FakeService service(kObjectSize);
  ManualExecutor executor;
  auto source = MakeSource(service, PrefetchSettings(), 0, executor.executor());
  ASSERT_THAT(source, NotNull());

  // A single read of most of the first window does not prefetch either.
  std::vector<char> buffer(1000);
  ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(executor.pending(), Eq(0U));
  EXPECT_THAT(source->Close(), IsOk());
  EXPECT_THAT(service.windows(), ElementsAre(WindowIs(0, 1024)));
}

TEST(AdaptiveObjectReadSource, PrefetchesNextWindowAtThreshold) {
  FakeService service(kObjectSize);
  ManualExecutor executor;
  auto source = MakeSource(service, PrefetchSettings(), 0, executor.executor());
  ASSERT_THAT(source, NotNull());

  std::string data;
  std::vector<char> buffer(256);
  auto read = [&] {
    auto r = source->Read(buffer.data(), buffer.size());
    ASSERT_THAT(r, IsOk());
    data.append(buffer.data(), r->bytes_received);
  };
  // The first window, [0, 1024), is never prefetched past.
  for (int i = 0; i != 4; ++i) read();
  EXPECT_THAT(executor.pending(), Eq(0U));
  // 768 of the 2048 bytes in the second window: below the threshold.
  for (int i = 0; i != 3; ++i) read();
  EXPECT_THAT(executor.pending(), Eq(0U));
  // 1024 bytes: the threshold.
  read();
  EXPECT_THAT(executor.pending(), Eq(1U));
  EXPECT_THAT(service.windows(),
              ElementsAre(WindowIs(0, 1024), WindowIs(1024, 2048)));

  executor.RunAll();
  EXPECT_THAT(service.windows(),
              ElementsAre(WindowIs(0, 1024), WindowIs(1024, 2048),
                          WindowIs(3072, 4096)));

  // Reading into the third window uses the prefetched window.
  for (int i = 0; i != 6; ++i) read();
  EXPECT_THAT(service.windows(),
              ElementsAre(WindowIs(0, 1024), WindowIs(1024, 2048),
                          WindowIs(3072, 4096)));

  // The rest of the object is read correctly, prefetching every window.
  while (source->IsOpen()) {
    executor.RunAll();
    read();
  }
  EXPECT_TRUE(data == service.contents());
  EXPECT_THAT(
      service.windows(),
      ElementsAre(WindowIs(0, 1024), WindowIs(1024, 2048), WindowIs(3072, 4096),
                  WindowIs(7168, 8192), WindowIs(15360, 4640)));
}

TEST(AdaptiveObjectReadSource, OpensWindowWhenPrefetchHasNotStarted) {
  FakeService service(kObjectSize);
  ManualExecutor executor;
  SummaryRecorder recorder;
  auto source = MakeSource(service, PrefetchSettings(), 0, executor.executor(),
                           recorder.callback());
  ASSERT_THAT(source, NotNull());

  // Read past the threshold of the second window, [1024, 3072).
  std::vector<char> buffer(4096);
  ASSERT_THAT(source->Read(buffer.data(), 2124), IsOk());
  EXPECT_THAT(executor.pending(), Eq(1U));
  // The prefetch is still queued when the application needs the window: the
  // window is opened directly, the source does not wait for the queue. The
  // read stops before the threshold of the third window.
  ASSERT_THAT(source->Read(buffer.data(), 1048), IsOk());
  EXPECT_THAT(service.windows(),
              ElementsAre(WindowIs(0, 1024), WindowIs(1024, 2048),
                          WindowIs(3072, 4096)));
  // The abandoned prefetch does nothing when it eventually runs.
  executor.RunAll();
  EXPECT_THAT(service.windows(), SizeIs(3));

  EXPECT_THAT(source->Close(), IsOk());
  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  auto const& summary = recorder.summaries()[0];
  // The prefetch never requested its window.
  EXPECT_THAT(summary.prefetch_started, Eq(1));
  EXPECT_THAT(summary.prefetch_used, Eq(0));
  EXPECT_THAT(summary.prefetch_abandoned, Eq(0));
  EXPECT_THAT(summary.windows_opened, Eq(3));
}

TEST(AdaptiveObjectReadSource, PrefetchDisabledOutsideInterval) {
  for (auto threshold :
       {0.0, 1.0, -1.0, 2.0, std::numeric_limits<double>::quiet_NaN()}) {
    SCOPED_TRACE("threshold=" + std::to_string(threshold));
    FakeService service(kObjectSize);
    int calls = 0;
    auto executor = [&calls](std::function<void()>) {
      ++calls;
      return true;
    };
    auto settings = TestSettings();
    settings.prefetch_threshold = threshold;
    auto source = MakeSource(service, settings, 0, executor);
    ASSERT_THAT(source, NotNull());
    auto const actual = ReadAll(*source, 256);
    EXPECT_THAT(actual.status, IsOk());
    EXPECT_TRUE(actual.data == service.contents());
    EXPECT_THAT(calls, Eq(0));
  }
}

TEST(AdaptiveObjectReadSource, PrefetchRejectedByExecutor) {
  FakeService service(kObjectSize);
  int calls = 0;
  auto executor = [&calls](std::function<void()>) {
    ++calls;
    return false;
  };
  auto source = MakeSource(service, PrefetchSettings(), 0, executor);
  ASSERT_THAT(source, NotNull());
  auto const actual = ReadAll(*source, 256);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  EXPECT_THAT(calls, Ne(0));
  EXPECT_THAT(service.windows(), SizeIs(5));
}

TEST(AdaptiveObjectReadSource, PrefetchErrorIsReported) {
  FakeService service(kObjectSize);
  service.set_open_error(3072, PermanentError());
  ManualExecutor executor;
  auto source = MakeSource(service, PrefetchSettings(), 0, executor.executor());
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(4096);
  ASSERT_THAT(source->Read(buffer.data(), 2124), IsOk());
  executor.RunAll();
  // The data of the second window is returned, the error follows.
  auto r = source->Read(buffer.data(), 1024);
  ASSERT_THAT(r, IsOk());
  EXPECT_THAT(r->bytes_received, Eq(948U));
  EXPECT_TRUE(source->IsOpen());
  EXPECT_THAT(source->Read(buffer.data(), buffer.size()),
              StatusIs(PermanentError().code()));
  EXPECT_FALSE(source->IsOpen());
}

TEST(AdaptiveObjectReadSource, CloseReleasesPrefetchedWindow) {
  FakeService service(kObjectSize);
  ManualExecutor executor;
  SummaryRecorder recorder;
  auto source = MakeSource(service, PrefetchSettings(), 0, executor.executor(),
                           recorder.callback());
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(2124);
  ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  executor.RunAll();
  ASSERT_THAT(service.stats(), SizeIs(3));
  EXPECT_THAT(source->Close(), IsOk());

  auto const stats = service.stats();
  EXPECT_THAT(stats[2]->delivered.load(), Eq(0));
  EXPECT_TRUE(stats[2]->destroyed);

  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  auto const& summary = recorder.summaries()[0];
  EXPECT_THAT(summary.close_reason,
              Eq(AdaptiveReadSummary::CloseReason::kEarlyClose));
  EXPECT_THAT(summary.prefetch_started, Eq(1));
  EXPECT_THAT(summary.prefetch_used, Eq(0));
  EXPECT_THAT(summary.prefetch_abandoned, Eq(1));
  // The prefetched window was requested, even if it was not used.
  EXPECT_THAT(summary.windows_opened, Eq(3));
  EXPECT_THAT(summary.bytes_requested, Eq(1024 + 2048 + 4096));
}

TEST(AdaptiveObjectReadSource, PrefetchesDuringLargeReads) {
  // A single `Read()` may span several windows, for example when the
  // application reads large blocks. Each window after the second is prefetched
  // while the previous one is read.
  FakeService service(kObjectSize);
  auto inline_executor = [](std::function<void()> task) {
    task();
    return true;
  };
  SummaryRecorder recorder;
  auto source = MakeSource(service, PrefetchSettings(), 0, inline_executor,
                           recorder.callback());
  ASSERT_THAT(source, NotNull());

  // The first read returns the size of the object, like the `peek()` in
  // `Client::ReadObject()`.
  std::string data(static_cast<std::size_t>(kObjectSize), '\0');
  auto r = source->Read(data.data(), 100);
  ASSERT_THAT(r, IsOk());
  ASSERT_THAT(r->bytes_received, Eq(100U));
  r = source->Read(data.data() + 100, data.size() - 100);
  ASSERT_THAT(r, IsOk());
  EXPECT_THAT(r->bytes_received,
              Eq(static_cast<std::size_t>(kObjectSize - 100)));
  EXPECT_FALSE(source->IsOpen());
  EXPECT_TRUE(data == service.contents());

  EXPECT_THAT(
      service.windows(),
      ElementsAre(WindowIs(0, 1024), WindowIs(1024, 2048), WindowIs(3072, 4096),
                  WindowIs(7168, 8192), WindowIs(15360, 4640)));
  // The second window is opened once the first window completes, every window
  // after that was opened before the previous window completed, that is, it
  // was prefetched.
  EXPECT_THAT(service.previous_completed(),
              ElementsAre(true, true, false, false, false));

  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  auto const& summary = recorder.summaries()[0];
  EXPECT_THAT(summary.prefetch_started, Eq(3));
  EXPECT_THAT(summary.prefetch_used, Eq(3));
  EXPECT_THAT(summary.prefetch_abandoned, Eq(0));
  EXPECT_THAT(summary.windows_opened, Eq(5));
  EXPECT_THAT(summary.bytes_requested, Eq(kObjectSize));
}

TEST(AdaptiveObjectReadSource, DestructorDoesNotWaitForPrefetch) {
  FakeService service(kObjectSize);
  std::promise<void> entered;
  auto entered_future = entered.get_future();
  std::promise<void> release;
  auto release_future = release.get_future().share();
  service.set_on_open([&entered, release_future](Window const& w) {
    if (w.offset != 3072) return;
    entered.set_value();
    release_future.wait();
  });
  std::vector<std::thread> workers;
  auto executor = [&workers](std::function<void()> task) {
    workers.emplace_back(std::move(task));
    return true;
  };
  auto source = MakeSource(service, PrefetchSettings(), 0, executor);
  ASSERT_THAT(source, NotNull());

  // No assertion may return early from here on: the prefetch must be released
  // and joined on all paths.
  std::vector<char> buffer(2124);
  EXPECT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  auto const blocked = entered_future.wait_for(std::chrono::seconds(30));
  EXPECT_THAT(blocked, Eq(std::future_status::ready));
  // The prefetch is blocked opening the window, destroying the source must not
  // wait for it. Destroy it in a separate thread, so a regression fails the
  // test instead of hanging it.
  auto destroyed =
      std::async(std::launch::async, [&source] { source.reset(); });
  EXPECT_THAT(destroyed.wait_for(std::chrono::seconds(30)),
              Eq(std::future_status::ready));
  release.set_value();
  destroyed.wait();
  for (auto& w : workers) w.join();

  // The window opened by the abandoned prefetch is released.
  auto const stats = service.stats();
  ASSERT_THAT(stats, SizeIs(3));
  EXPECT_THAT(stats[2]->delivered.load(), Eq(0));
  EXPECT_TRUE(stats[2]->destroyed);
}

TEST(AdaptiveObjectReadSource, PrefetchOnThreadPool) {
  FakeService service(200 * 1024);
  auto pool = std::make_shared<ThreadPool>(4);
  auto executor = [pool](std::function<void()> task) {
    return pool->Enqueue(std::move(task));
  };
  auto settings = PrefetchSettings();
  settings.max_window = 16 * 1024;
  auto source = MakeSource(service, settings, 0, executor);
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 700);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  // The windows tile the object, without gaps or overlaps.
  std::int64_t expected_offset = 0;
  for (auto const& w : service.windows()) {
    EXPECT_THAT(w.offset, Eq(expected_offset));
    expected_offset = w.offset + w.length;
  }
  EXPECT_THAT(expected_offset, Eq(200 * 1024));
}

TEST(AdaptiveObjectReadSource, EmptyObjectFallsBackToUnboundedDownload) {
  FakeService service(0);
  auto unbounded = std::make_unique<MockObjectReadSource>();
  auto* expected = unbounded.get();
  auto open_unbounded = [&unbounded] {
    return StatusOr<std::unique_ptr<ObjectReadSource>>(std::move(unbounded));
  };
  auto source =
      MakeAdaptiveObjectReadSource(service.Factory(), open_unbounded,
                                   Executor{}, TestSettings(), 0, std::nullopt);
  ASSERT_THAT(source, IsOk());
  EXPECT_THAT(source->get(), Eq(expected));
  EXPECT_THAT(service.windows(), ElementsAre(WindowIs(0, 1024)));
}

TEST(AdaptiveObjectReadSource, OutOfRangeFromOffsetIsAnError) {
  // A regular download from past the end of the object fails too.
  FakeService service(100);
  auto open_unbounded = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ADD_FAILURE() << "unexpected call";
    return google::cloud::internal::UnknownError("unexpected");
  };
  auto source = MakeAdaptiveObjectReadSource(service.Factory(), open_unbounded,
                                             Executor{}, TestSettings(), 100,
                                             std::nullopt);
  EXPECT_THAT(source, StatusIs(StatusCode::kOutOfRange));
}

TEST(AdaptiveObjectReadSource, FirstWindowErrorIsReturned) {
  FakeService service(kObjectSize);
  service.set_open_error(0, PermanentError());
  auto open_unbounded = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ADD_FAILURE() << "unexpected call";
    return google::cloud::internal::UnknownError("unexpected");
  };
  auto source =
      MakeAdaptiveObjectReadSource(service.Factory(), open_unbounded,
                                   Executor{}, TestSettings(), 0, std::nullopt);
  EXPECT_THAT(source, StatusIs(PermanentError().code()));
}

TEST(AdaptiveObjectReadSource, ShortWindowWithoutSizeIsEndOfObject) {
  FakeService service(1500);
  service.set_grpc(true).set_report_size(false);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 512);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  EXPECT_THAT(service.windows(),
              ElementsAre(WindowIs(0, 1024), WindowIs(1024, 2048)));
}

TEST(AdaptiveObjectReadSource, OutOfRangeWithoutSizeIsEndOfObject) {
  // The object ends exactly at a window boundary: only the next request finds
  // the end of the object.
  FakeService service(3072);
  service.set_grpc(true).set_report_size(false);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 512);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  EXPECT_THAT(service.windows(),
              ElementsAre(WindowIs(0, 1024), WindowIs(1024, 2048),
                          WindowIs(3072, 4096)));
}

TEST(AdaptiveObjectReadSource, ShortWindowWithSizeContinuesAtOffset) {
  FakeService service(kObjectSize);
  service.set_short_window(1024, 1000);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 512);
  EXPECT_THAT(actual.status, IsOk());
  EXPECT_TRUE(actual.data == service.contents());
  auto const windows = service.windows();
  ASSERT_THAT(windows, SizeIs(5));
  EXPECT_THAT(windows[2], WindowIs(2024, 4096));
}

TEST(AdaptiveObjectReadSource, EmptyWindowWithSizeIsAnError) {
  FakeService service(kObjectSize);
  service.set_short_window(1024, 0);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 4096);
  EXPECT_THAT(actual.status, StatusIs(StatusCode::kInternal,
                                      HasSubstr("ended without any data")));
  EXPECT_TRUE(actual.data == service.contents().substr(0, 1024));
}

TEST(AdaptiveObjectReadSource, WindowOpenErrorIsReported) {
  FakeService service(kObjectSize);
  service.set_open_error(1024, PermanentError());
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(1024);
  auto r = source->Read(buffer.data(), buffer.size());
  ASSERT_THAT(r, IsOk());
  EXPECT_THAT(r->bytes_received, Eq(1024U));
  EXPECT_TRUE(source->IsOpen());
  EXPECT_THAT(source->Read(buffer.data(), buffer.size()),
              StatusIs(PermanentError().code()));
  EXPECT_FALSE(source->IsOpen());
  EXPECT_THAT(source->Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kFailedPrecondition));
}

TEST(AdaptiveObjectReadSource, WindowReadErrorAfterDataIsReportedOnNextRead) {
  auto second = std::make_unique<MockObjectReadSource>();
  EXPECT_CALL(*second, IsOpen).WillRepeatedly(Return(true));
  EXPECT_CALL(*second, Read)
      .WillOnce(Return(google::cloud::internal::UnavailableError("try again")));
  FakeService service(kObjectSize);
  auto service_factory = service.Factory();
  auto factory = std::make_shared<WindowFactory const>(
      [&](Window const& w) -> StatusOr<std::unique_ptr<ObjectReadSource>> {
        if (w.offset == 1024) {
          return std::unique_ptr<ObjectReadSource>(std::move(second));
        }
        return (*service_factory)(w);
      });
  auto source = MakeAdaptiveObjectReadSource(factory, {}, Executor{},
                                             TestSettings(), 0, std::nullopt);
  ASSERT_THAT(source, IsOk());

  std::vector<char> buffer(4096);
  auto r = (*source)->Read(buffer.data(), buffer.size());
  ASSERT_THAT(r, IsOk());
  EXPECT_THAT(r->bytes_received, Eq(1024U));
  EXPECT_THAT(r->response.status_code, Eq(HttpStatusCode::kContinue));
  EXPECT_TRUE((*source)->IsOpen());
  EXPECT_THAT((*source)->Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kUnavailable));
  EXPECT_FALSE((*source)->IsOpen());
}

TEST(AdaptiveObjectReadSource, TooMuchDataIsAnError) {
  auto first = std::make_unique<MockObjectReadSource>();
  EXPECT_CALL(*first, IsOpen).WillRepeatedly(Return(true));
  EXPECT_CALL(*first, Read).WillOnce([](char*, std::size_t n) {
    return ReadSourceResult{n + 1,
                            HttpResponse{HttpStatusCode::kContinue, {}, {}}};
  });
  auto factory = std::make_shared<WindowFactory const>(
      [](Window const&) -> StatusOr<std::unique_ptr<ObjectReadSource>> {
        return google::cloud::internal::UnknownError("unexpected");
      });
  AdaptiveObjectReadSource source(
      factory, AdaptiveObjectReadSource::UnboundedFactory{}, Executor{},
      TestSettings(), Window{0, 1024, std::nullopt}, std::move(first));
  std::vector<char> buffer(4096);
  EXPECT_THAT(source.Read(buffer.data(), 100),
              StatusIs(StatusCode::kInternal, HasSubstr("more data")));
  EXPECT_FALSE(source.IsOpen());
}

TEST(AdaptiveObjectReadSource, StalledWindowIsAnError) {
  auto first = std::make_unique<MockObjectReadSource>();
  EXPECT_CALL(*first, IsOpen).WillRepeatedly(Return(true));
  EXPECT_CALL(*first, Read)
      .WillOnce(Return(ReadSourceResult{
          0, HttpResponse{HttpStatusCode::kContinue, {}, {}}}));
  auto factory = std::make_shared<WindowFactory const>(
      [](Window const&) -> StatusOr<std::unique_ptr<ObjectReadSource>> {
        return google::cloud::internal::UnknownError("unexpected");
      });
  AdaptiveObjectReadSource source(
      factory, AdaptiveObjectReadSource::UnboundedFactory{}, Executor{},
      TestSettings(), Window{0, 1024, std::nullopt}, std::move(first));
  std::vector<char> buffer(100);
  EXPECT_THAT(source.Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kInternal, HasSubstr("progress")));
}

TEST(AdaptiveObjectReadSource, ZeroByteReadDoesNothing) {
  auto first = std::make_unique<MockObjectReadSource>();
  EXPECT_CALL(*first, Read).Times(0);
  auto factory = std::make_shared<WindowFactory const>(
      [](Window const&) -> StatusOr<std::unique_ptr<ObjectReadSource>> {
        return google::cloud::internal::UnknownError("unexpected");
      });
  AdaptiveObjectReadSource source(
      factory, AdaptiveObjectReadSource::UnboundedFactory{}, Executor{},
      TestSettings(), Window{0, 1024, std::nullopt}, std::move(first));
  char c;
  auto r = source.Read(&c, 0);
  ASSERT_THAT(r, IsOk());
  EXPECT_THAT(r->bytes_received, Eq(0U));
}

TEST(AdaptiveObjectReadSource, CloseDrainsSmallRemainder) {
  // Reading the rest of a small window completes the response: the service
  // does not log a cancelled download, and the connection can be reused.
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(100);
  ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(source->Close(), IsOk());
  EXPECT_FALSE(source->IsOpen());

  auto const stats = service.stats();
  ASSERT_THAT(stats, SizeIs(1));
  EXPECT_THAT(stats[0]->delivered.load(), Eq(1024));
  EXPECT_TRUE(stats[0]->completed);
  EXPECT_TRUE(stats[0]->closed);
  EXPECT_THAT(source->Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kFailedPrecondition));
}

TEST(AdaptiveObjectReadSource, CloseAbandonsLargeRemainder) {
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  // Read into the third window, [3072, 7168), leaving more than the initial
  // window unread.
  std::vector<char> buffer(3172);
  ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(source->Close(), IsOk());

  auto const stats = service.stats();
  ASSERT_THAT(stats, SizeIs(3));
  EXPECT_THAT(stats[2]->delivered.load(), Eq(100));
  EXPECT_FALSE(stats[2]->completed);
  EXPECT_TRUE(stats[2]->closed);
}

TEST(AdaptiveObjectReadSource, CloseAfterEndOfObject) {
  FakeService service(100);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  EXPECT_THAT(ReadAll(*source, 512).status, IsOk());
  auto response = source->Close();
  ASSERT_THAT(response, IsOk());
  EXPECT_THAT(response->status_code, Eq(HttpStatusCode::kOk));
}

TEST(AdaptiveObjectReadSource, CloseTranscodedDownloadDoesNotDrain) {
  FakeService service(kObjectSize);
  service.set_behavior(Behavior::kTranscode);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(100);
  ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(source->Close(), IsOk());
  // The regular download has no bound, it is closed without reading the rest.
  auto const stats = service.unbounded_stats();
  ASSERT_THAT(stats, SizeIs(1));
  EXPECT_THAT(stats[0]->delivered.load(), Eq(100));
  EXPECT_FALSE(stats[0]->completed);
  EXPECT_TRUE(stats[0]->closed);
}

TEST(AdaptiveObjectReadSource, DrainUsesMaxDrainNotInitialWindow) {
  // The remainder of the third window, [3072, 7168), is larger than the first
  // window, but within `max_drain`.
  FakeService service(kObjectSize);
  auto settings = TestSettings();
  settings.max_drain = 4096;
  SummaryRecorder recorder;
  auto source =
      MakeSource(service, settings, 0, Executor{}, recorder.callback());
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(3172);
  ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(source->Close(), IsOk());

  auto const stats = service.stats();
  ASSERT_THAT(stats, SizeIs(3));
  EXPECT_THAT(stats[2]->delivered.load(), Eq(4096));
  EXPECT_TRUE(stats[2]->completed);
  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  EXPECT_THAT(recorder.summaries()[0].bytes_drained, Eq(3996));
  EXPECT_THAT(recorder.summaries()[0].windows_abandoned, Eq(0));
}

TEST(AdaptiveObjectReadSource, AbandonsRemainderAboveMaxDrain) {
  // The remainder of the first window is within `initial_window`, but larger
  // than `max_drain`.
  FakeService service(kObjectSize);
  auto settings = TestSettings();
  settings.max_drain = 512;
  SummaryRecorder recorder;
  auto source =
      MakeSource(service, settings, 0, Executor{}, recorder.callback());
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(100);
  ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(source->Close(), IsOk());

  auto const stats = service.stats();
  ASSERT_THAT(stats, SizeIs(1));
  EXPECT_THAT(stats[0]->delivered.load(), Eq(100));
  EXPECT_FALSE(stats[0]->completed);
  EXPECT_TRUE(stats[0]->closed);
  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  EXPECT_THAT(recorder.summaries()[0].bytes_drained, Eq(0));
  EXPECT_THAT(recorder.summaries()[0].windows_abandoned, Eq(1));
}

TEST(AdaptiveObjectReadSource, ZeroMaxDrainDisablesDrain) {
  FakeService service(kObjectSize);
  auto settings = TestSettings();
  settings.max_drain = 0;
  auto source = MakeSource(service, settings, 0, Executor{});
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(1000);
  ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(source->Close(), IsOk());
  auto const stats = service.stats();
  ASSERT_THAT(stats, SizeIs(1));
  EXPECT_FALSE(stats[0]->completed);
}

TEST(AdaptiveObjectReadSource, SummaryAtEndOfData) {
  FakeService service(kObjectSize);
  SummaryRecorder recorder;
  auto source =
      MakeSource(service, TestSettings(), 0, Executor{}, recorder.callback());
  ASSERT_THAT(source, NotNull());

  EXPECT_THAT(ReadAll(*source, 512).status, IsOk());
  // The summary is reported with the last read.
  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  EXPECT_THAT(source->Close(), IsOk());
  ASSERT_THAT(recorder.summaries(), SizeIs(1));

  auto const& summary = recorder.summaries()[0];
  EXPECT_THAT(summary.close_reason,
              Eq(AdaptiveReadSummary::CloseReason::kEndOfData));
  EXPECT_THAT(summary.bytes_delivered, Eq(kObjectSize));
  EXPECT_THAT(summary.initial_window, Eq(1024));
  EXPECT_THAT(summary.windows_opened, Eq(5));
  EXPECT_THAT(summary.bytes_requested, Eq(kObjectSize));
  EXPECT_THAT(summary.bytes_drained, Eq(0));
  EXPECT_THAT(summary.windows_abandoned, Eq(0));
  EXPECT_THAT(summary.prefetch_started, Eq(0));
  EXPECT_FALSE(summary.unbounded_fallback);
}

TEST(AdaptiveObjectReadSource, SummaryOnEarlyClose) {
  FakeService service(kObjectSize);
  SummaryRecorder recorder;
  auto source =
      MakeSource(service, TestSettings(), 0, Executor{}, recorder.callback());
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(100);
  ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(recorder.summaries(), IsEmpty());
  EXPECT_THAT(source->Close(), IsOk());
  source.reset();

  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  auto const& summary = recorder.summaries()[0];
  EXPECT_THAT(summary.close_reason,
              Eq(AdaptiveReadSummary::CloseReason::kEarlyClose));
  EXPECT_THAT(summary.bytes_delivered, Eq(100));
  EXPECT_THAT(summary.windows_opened, Eq(1));
  EXPECT_THAT(summary.bytes_requested, Eq(1024));
  EXPECT_THAT(summary.bytes_drained, Eq(924));
  EXPECT_THAT(summary.windows_abandoned, Eq(0));
}

TEST(AdaptiveObjectReadSource, SummaryOnDestructionWithoutClose) {
  FakeService service(kObjectSize);
  SummaryRecorder recorder;
  auto source =
      MakeSource(service, TestSettings(), 0, Executor{}, recorder.callback());
  ASSERT_THAT(source, NotNull());

  std::vector<char> buffer(3172);
  ASSERT_THAT(source->Read(buffer.data(), buffer.size()), IsOk());
  source.reset();

  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  auto const& summary = recorder.summaries()[0];
  EXPECT_THAT(summary.close_reason,
              Eq(AdaptiveReadSummary::CloseReason::kEarlyClose));
  EXPECT_THAT(summary.bytes_delivered, Eq(3172));
  EXPECT_THAT(summary.windows_opened, Eq(3));
  EXPECT_THAT(summary.bytes_requested, Eq(1024 + 2048 + 4096));
  EXPECT_THAT(summary.windows_abandoned, Eq(1));
}

TEST(AdaptiveObjectReadSource, SummaryOnError) {
  FakeService service(kObjectSize);
  service.set_open_error(1024, PermanentError());
  SummaryRecorder recorder;
  auto source =
      MakeSource(service, TestSettings(), 0, Executor{}, recorder.callback());
  ASSERT_THAT(source, NotNull());

  auto const actual = ReadAll(*source, 4096);
  EXPECT_THAT(actual.status, StatusIs(PermanentError().code()));
  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  EXPECT_THAT(source->Close(), IsOk());
  ASSERT_THAT(recorder.summaries(), SizeIs(1));

  auto const& summary = recorder.summaries()[0];
  EXPECT_THAT(summary.close_reason,
              Eq(AdaptiveReadSummary::CloseReason::kError));
  EXPECT_THAT(summary.bytes_delivered, Eq(1024));
  EXPECT_THAT(summary.windows_opened, Eq(1));
}

TEST(AdaptiveObjectReadSource, SummaryOnCloseWithPendingError) {
  FakeService service(kObjectSize);
  service.set_open_error(1024, PermanentError());
  SummaryRecorder recorder;
  auto source =
      MakeSource(service, TestSettings(), 0, Executor{}, recorder.callback());
  ASSERT_THAT(source, NotNull());

  // The data is returned, the error is pending.
  std::vector<char> buffer(4096);
  auto r = source->Read(buffer.data(), buffer.size());
  ASSERT_THAT(r, IsOk());
  EXPECT_THAT(r->bytes_received, Eq(1024U));
  EXPECT_THAT(source->Close(), IsOk());
  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  EXPECT_THAT(recorder.summaries()[0].close_reason,
              Eq(AdaptiveReadSummary::CloseReason::kError));
}

TEST(AdaptiveObjectReadSource, SummaryForTranscodedObject) {
  FakeService service(kObjectSize);
  service.set_behavior(Behavior::kTranscode);
  SummaryRecorder recorder;
  auto source =
      MakeSource(service, TestSettings(), 0, Executor{}, recorder.callback());
  ASSERT_THAT(source, NotNull());

  EXPECT_THAT(ReadAll(*source, 4096).status, IsOk());
  ASSERT_THAT(recorder.summaries(), SizeIs(1));
  auto const& summary = recorder.summaries()[0];
  EXPECT_THAT(summary.close_reason,
              Eq(AdaptiveReadSummary::CloseReason::kEndOfData));
  EXPECT_TRUE(summary.unbounded_fallback);
  EXPECT_THAT(summary.bytes_delivered, Eq(kObjectSize));
  // The first window was discarded.
  EXPECT_THAT(summary.windows_abandoned, Eq(1));
}

TEST(AdaptiveReadSummary, Format) {
  AdaptiveReadSummary summary;
  summary.close_reason = AdaptiveReadSummary::CloseReason::kEndOfData;
  summary.bytes_delivered = 100;
  summary.initial_window = 512;
  summary.windows_opened = 2;
  summary.bytes_requested = 1536;
  summary.bytes_drained = 3;
  summary.windows_abandoned = 4;
  summary.prefetch_started = 5;
  summary.prefetch_used = 6;
  summary.prefetch_abandoned = 7;
  EXPECT_EQ(FormatAdaptiveReadSummary(summary),
            "close_reason=eof bytes_delivered=100 initial_window=512"
            " windows_opened=2 bytes_requested=1536 bytes_drained=3"
            " windows_abandoned=4 prefetch_started=5 prefetch_used=6"
            " prefetch_abandoned=7 unbounded_fallback=false");
  EXPECT_THAT(ToString(AdaptiveReadSummary::CloseReason::kEarlyClose),
              Eq(std::string("early_close")));
  EXPECT_THAT(ToString(AdaptiveReadSummary::CloseReason::kError),
              Eq(std::string("error")));
}

TEST(AdaptiveReadSummary, SpanAttributes) {
  auto span_catcher = testing_util::InstallSpanCatcher();
  AdaptiveReadSummary summary;
  summary.close_reason = AdaptiveReadSummary::CloseReason::kEarlyClose;
  summary.bytes_delivered = 100;
  summary.initial_window = 512;
  summary.windows_opened = 2;
  summary.bytes_requested = 1536;
  summary.bytes_drained = 412;
  summary.prefetch_started = 1;
  summary.prefetch_abandoned = 1;
  {
    auto span = google::cloud::internal::MakeSpan("test-span");
    auto scope = opentelemetry::trace::Scope(span);
    AddAdaptiveReadSpanAttributes(summary);
    span->End();
  }
  using testing_util::OTelAttribute;
  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          testing_util::SpanNamed("test-span"),
          testing_util::SpanHasAttributes(
              OTelAttribute<std::string>("gcs.adaptive_read.close_reason",
                                         "early_close"),
              OTelAttribute<std::int64_t>("gcs.adaptive_read.bytes_delivered",
                                          100),
              OTelAttribute<std::int64_t>("gcs.adaptive_read.initial_window",
                                          512),
              OTelAttribute<std::int64_t>("gcs.adaptive_read.windows_opened",
                                          2),
              OTelAttribute<std::int64_t>("gcs.adaptive_read.bytes_requested",
                                          1536),
              OTelAttribute<std::int64_t>("gcs.adaptive_read.bytes_drained",
                                          412),
              OTelAttribute<std::int64_t>("gcs.adaptive_read.windows_abandoned",
                                          0),
              OTelAttribute<std::int64_t>("gcs.adaptive_read.prefetch_started",
                                          1),
              OTelAttribute<std::int64_t>("gcs.adaptive_read.prefetch_used", 0),
              OTelAttribute<std::int64_t>(
                  "gcs.adaptive_read.prefetch_abandoned", 1),
              OTelAttribute<bool>("gcs.adaptive_read.unbounded_fallback",
                                  false)))));
}

// Reads all the data in @p buf. Errors are reported as exceptions when they
// are enabled, and through `buf.status()` otherwise.
std::string ReadAllFromStreambuf(ObjectReadStreambuf& buf) {
  std::string data;
  std::vector<char> buffer(3000);
  while (true) {
    auto const n = buf.sgetn(buffer.data(), buffer.size());
    if (n <= 0) break;
    data.append(buffer.data(), static_cast<std::size_t>(n));
  }
  return data;
}

TEST(AdaptiveObjectReadSource, StreambufValidatesChecksumsAcrossWindows) {
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  ObjectReadStreambuf buf(ReadObjectRangeRequest("test-bucket", "test-object"),
                          std::move(source));

  auto const data = ReadAllFromStreambuf(buf);
  EXPECT_TRUE(data == service.contents());
  EXPECT_THAT(buf.status(), IsOk());
  EXPECT_FALSE(buf.IsOpen());
  // The checksums computed over all the windows match the checksums of the
  // full object.
  EXPECT_THAT(buf.received_hash(), HasSubstr(service.hashes().crc32c));
  EXPECT_THAT(buf.computed_hash(), Eq(buf.received_hash()));
  EXPECT_THAT(buf.size(), Optional(Eq(std::uint64_t{kObjectSize})));
  EXPECT_THAT(buf.generation(), Optional(kGeneration));
  EXPECT_THAT(service.windows(), SizeIs(5));
}

TEST(AdaptiveObjectReadSource, StreambufValidatesChecksumsWithMD5) {
  FakeService service(kObjectSize);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  google::cloud::internal::OptionsSpan const span(
      Options{}.set<DownloadChecksumValidationOption>(
          ChecksumAlgorithm::kCrc32cAndMD5));
  ObjectReadStreambuf buf(ReadObjectRangeRequest("test-bucket", "test-object"),
                          std::move(source));

  auto const data = ReadAllFromStreambuf(buf);
  EXPECT_TRUE(data == service.contents());
  EXPECT_THAT(buf.status(), IsOk());
  EXPECT_THAT(buf.received_hash(), HasSubstr(service.hashes().crc32c));
  EXPECT_THAT(buf.received_hash(), HasSubstr(service.hashes().md5));
  EXPECT_THAT(buf.computed_hash(), Eq(buf.received_hash()));
}

TEST(AdaptiveObjectReadSource, StreambufDetectsCorruptionInLaterWindow) {
  FakeService service(kObjectSize);
  service.set_corrupt_offset(12345);
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  ObjectReadStreambuf buf(ReadObjectRangeRequest("test-bucket", "test-object"),
                          std::move(source));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ReadAllFromStreambuf(buf), HashMismatchError);
#else
  ReadAllFromStreambuf(buf);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THAT(buf.status(), StatusIs(StatusCode::kDataLoss));
  EXPECT_THAT(buf.received_hash(), HasSubstr(service.hashes().crc32c));
  EXPECT_THAT(buf.computed_hash(), Ne(buf.received_hash()));
}

TEST(AdaptiveObjectReadSource, StreambufDetectsMismatchedObjectHash) {
  FakeService service(kObjectSize);
  service.set_hashes(HashValues{ComputeCrc32cChecksum("not the object"),
                                ComputeMD5Hash("not the object")});
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  ObjectReadStreambuf buf(ReadObjectRangeRequest("test-bucket", "test-object"),
                          std::move(source));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ReadAllFromStreambuf(buf), HashMismatchError);
#else
  ReadAllFromStreambuf(buf);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THAT(buf.status(), StatusIs(StatusCode::kDataLoss));
}

TEST(AdaptiveObjectReadSource, StreambufDoesNotValidateDownloadsFromOffset) {
  // The service reports the checksums of the full object, a download that
  // starts at an offset cannot be validated. This is unchanged by adaptive
  // reads.
  FakeService service(kObjectSize);
  service.set_corrupt_offset(12345);
  auto source = MakeSource(service, TestSettings(), 5000, Executor{});
  ASSERT_THAT(source, NotNull());
  ObjectReadStreambuf buf(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_option(ReadFromOffset(5000)),
                          std::move(source));

  auto const data = ReadAllFromStreambuf(buf);
  EXPECT_THAT(data.size(), Eq(static_cast<std::size_t>(kObjectSize - 5000)));
  EXPECT_THAT(buf.status(), IsOk());
  EXPECT_THAT(buf.computed_hash(), IsEmpty());
}

TEST(AdaptiveObjectReadSource, StreambufDoesNotValidateTranscodedDownloads) {
  // The service reports the checksums of the stored (compressed) data, the
  // decompressed data does not match them.
  FakeService service(kObjectSize);
  service.set_behavior(Behavior::kTranscode)
      .set_hashes(HashValues{ComputeCrc32cChecksum("compressed"),
                             ComputeMD5Hash("compressed")});
  auto source = MakeSource(service, TestSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  ObjectReadStreambuf buf(ReadObjectRangeRequest("test-bucket", "test-object"),
                          std::move(source));

  auto const data = ReadAllFromStreambuf(buf);
  EXPECT_TRUE(data == service.contents());
  EXPECT_THAT(buf.status(), IsOk());
  EXPECT_THAT(buf.transformation(), Optional(Eq("gunzipped")));
}

TEST(AdaptiveObjectReadSource, StreambufIstreamReadsAcrossWindows) {
  FakeService service(kObjectSize);
  auto source = MakeSource(service, PrefetchSettings(), 0, Executor{});
  ASSERT_THAT(source, NotNull());
  ObjectReadStreambuf buf(ReadObjectRangeRequest("test-bucket", "test-object"),
                          std::move(source));
  std::istream stream(&buf);

  // Each `read()` must be satisfied in full, even across window boundaries.
  std::string data;
  std::vector<char> buffer(3000);
  while (stream.read(buffer.data(), buffer.size())) {
    data.append(buffer.data(), buffer.size());
  }
  data.append(buffer.data(), static_cast<std::size_t>(stream.gcount()));
  EXPECT_TRUE(data == service.contents());
  EXPECT_TRUE(stream.eof());
  EXPECT_THAT(buf.status(), IsOk());
  EXPECT_THAT(buf.computed_hash(), Eq(buf.received_hash()));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
