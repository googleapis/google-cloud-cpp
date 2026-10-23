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
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

auto constexpr kApiClientFeature = "gccl-feat/adaptive-read";

// The largest buffer used to read and discard data.
std::int64_t constexpr kMaxDiscardBuffer = 128 * 1024;

ReadSourceResult EmptyResult() {
  return ReadSourceResult{0, HttpResponse{HttpStatusCode::kContinue, {}, {}}};
}

// Returns the size of the object from a `content-range: bytes b-e/size`
// header, if the response has one.
std::optional<std::int64_t> SizeFromContentRange(
    std::multimap<std::string, std::string> const& headers) {
  auto const f = headers.find("content-range");
  if (f == headers.end()) return std::nullopt;
  auto const pos = f->second.find_last_of('/');
  if (pos == std::string::npos) return std::nullopt;
  // The size is `*` when the service does not know it, `SimpleAtoi()` rejects
  // that value.
  std::int64_t size = 0;
  if (!absl::SimpleAtoi(absl::string_view(f->second).substr(pos + 1), &size)) {
    return std::nullopt;
  }
  if (size < 0) return std::nullopt;
  return size;
}

// Returns true if @p data, a read before any data of the window was
// delivered, shows that the service did not honor the `Range` header of the
// request. @p more_data is true if the response has more data.
//
// The service ignores byte ranges when it transforms the object (e.g.
// decompressive transcoding), and returns the full object. Otherwise, a REST
// response to a range request always includes a `content-range` header.
//
// REST responses return their headers with the first read. gRPC responses
// return their headers (the call metadata) with the last read, which may also
// return data, and gRPC always honors the requested range. Headers without
// `content-range` show an ignored range only if they come from a REST
// response: the response has more data, or it completed with `200 OK` (all
// gRPC reads report `kContinue`).
bool IgnoresRange(ReadSourceResult const& data, bool more_data) {
  if (data.transformation.has_value()) return true;
  auto const& headers = data.response.headers;
  if (headers.empty() || headers.count("content-range") != 0) return false;
  return more_data || data.response.status_code == HttpStatusCode::kOk;
}

// Appends the data and metadata of @p data to @p result.
void MergeResult(ReadSourceResult& result, ReadSourceResult data) {
  result.bytes_received += data.bytes_received;
  for (auto& kv : data.response.headers) {
    result.response.headers.emplace(kv.first, std::move(kv.second));
  }
  result.hashes = Merge(std::move(result.hashes), std::move(data.hashes));
  if (!result.generation) result.generation = data.generation;
  if (!result.metageneration) result.metageneration = data.metageneration;
  if (!result.storage_class)
    result.storage_class = std::move(data.storage_class);
  if (!result.size) result.size = data.size;
  if (!result.transformation) {
    result.transformation = std::move(data.transformation);
  }
}

// Reads and discards data until @p source is closed.
//
// Returns the metadata (headers, hashes, etc.) of the responses, without any
// data. Fails if @p source returns more than @p limit bytes, or if it stops
// making progress without closing.
StatusOr<ReadSourceResult> Discard(ObjectReadSource& source,
                                   std::int64_t limit) {
  std::vector<char> buffer(static_cast<std::size_t>(
      std::clamp<std::int64_t>(limit, 1, kMaxDiscardBuffer)));
  auto metadata = EmptyResult();
  std::int64_t discarded = 0;
  while (source.IsOpen()) {
    StatusOr<ReadSourceResult> r = source.Read(buffer.data(), buffer.size());
    if (!r) return std::move(r).status();
    auto const received = r->bytes_received;
    discarded += static_cast<std::int64_t>(received);
    if (discarded > limit) {
      return google::cloud::internal::InternalError(
          "the service returned more data than requested", GCP_ERROR_INFO());
    }
    metadata.response.status_code = r->response.status_code;
    r->bytes_received = 0;
    MergeResult(metadata, *std::move(r));
    if (received == 0 && source.IsOpen()) {
      return google::cloud::internal::InternalError(
          "the download stopped making progress", GCP_ERROR_INFO());
    }
  }
  return metadata;
}

bool PrefetchEnabled(AdaptiveReadSettings const& settings) {
  // NaN fails both comparisons, and disables prefetching.
  return settings.prefetch_threshold > 0.0 && settings.prefetch_threshold < 1.0;
}

}  // namespace

std::string AdaptiveReadApiClientHeader(Options const& options) {
  auto header = x_goog_api_client();
  if (options.get<AdaptiveReadAttributionOption>()) {
    absl::StrAppend(&header, " ", kApiClientFeature);
  }
  return header;
}

AdaptiveReadSettings MakeAdaptiveReadSettings(
    storage_experimental::AdaptiveReadRangeConfig const& config) {
  storage_experimental::AdaptiveReadRangeConfig const defaults;
  AdaptiveReadSettings settings;
  settings.initial_window = config.initial_window > 0 ? config.initial_window
                                                      : defaults.initial_window;
  settings.max_window = (std::max)(config.max_window, settings.initial_window);
  // NaN fails the comparison, and is replaced too.
  settings.growth_factor =
      config.growth_factor >= 1.0 ? config.growth_factor : 1.0;
  settings.prefetch_threshold = config.prefetch_threshold;
  return settings;
}

std::int64_t NextAdaptiveWindowSize(std::int64_t current,
                                    AdaptiveReadSettings const& settings) {
  if (current >= settings.max_window) return settings.max_window;
  auto const next = static_cast<double>(current) * settings.growth_factor;
  // This also handles a growth factor of +infinity.
  if (!(next < static_cast<double>(settings.max_window))) {
    return settings.max_window;
  }
  return (std::max)(current, static_cast<std::int64_t>(std::ceil(next)));
}

/**
 * The state of a prefetch, shared with the task that opens the window.
 *
 * The task may outlive the `AdaptiveObjectReadSource` that started it. A
 * source that no longer needs the window marks the prefetch as abandoned, and
 * the task then releases the window it opened (if any) instead of publishing
 * it. The source never waits for a prefetch that has not started.
 */
struct AdaptiveObjectReadSource::PrefetchState {
  explicit PrefetchState(Window w) : window(std::move(w)) {}

  Window const window;
  std::mutex mu;
  std::condition_variable cv;
  bool started = false;                                // GUARDED_BY(mu)
  bool done = false;                                   // GUARDED_BY(mu)
  bool abandoned = false;                              // GUARDED_BY(mu)
  StatusOr<std::unique_ptr<ObjectReadSource>> source;  // GUARDED_BY(mu)
};

AdaptiveObjectReadSource::AdaptiveObjectReadSource(
    std::shared_ptr<WindowFactory const> factory,
    UnboundedFactory open_unbounded, Executor executor,
    AdaptiveReadSettings settings, Window first_window,
    std::unique_ptr<ObjectReadSource> first_source)
    : factory_(std::move(factory)),
      open_unbounded_(std::move(open_unbounded)),
      executor_(std::move(executor)),
      settings_(settings),
      start_offset_(first_window.offset),
      offset_(first_window.offset),
      generation_(first_window.generation),
      window_(std::move(first_window)),
      window_nominal_(settings_.initial_window),
      source_(std::move(first_source)) {}

AdaptiveObjectReadSource::~AdaptiveObjectReadSource() { AbandonPrefetch(); }

bool AdaptiveObjectReadSource::IsOpen() const {
  if (closed_) return false;
  // The caller must call `Read()` again to receive the error.
  if (!pending_error_.ok()) return true;
  if (pass_through_) return source_ && source_->IsOpen();
  return !eof_;
}

StatusOr<HttpResponse> AdaptiveObjectReadSource::Close() {
  closed_ = true;
  AbandonPrefetch();
  auto source = std::move(source_);
  if (!source) return HttpResponse{HttpStatusCode::kOk, {}, {}};
  if (!pass_through_ && source->IsOpen()) {
    // Reading the rest of a small window completes the request. That costs
    // little, as the service has most likely sent these bytes already. And it
    // avoids tearing down the connection, which the service would also log as
    // a cancelled download. Larger windows are abandoned. Any error is ignored:
    // the application is no longer interested in the data.
    auto const remaining = WindowEnd() - offset_;
    if (remaining <= settings_.initial_window) {
      (void)Discard(*source, remaining);
    }
  }
  return source->Close();
}

StatusOr<ReadSourceResult> AdaptiveObjectReadSource::Read(char* buf,
                                                          std::size_t n) {
  if (closed_) {
    return google::cloud::internal::FailedPreconditionError(
        "Stream is not open", GCP_ERROR_INFO());
  }
  auto result = EmptyResult();
  if (!pending_error_.ok()) {
    auto status = std::exchange(pending_error_, Status{});
    return Fail(std::move(result), std::move(status));
  }
  if (n == 0) return result;
  if (pass_through_) return ReadPassThrough(buf, n, std::move(result));
  return ReadWindows(buf, n, std::move(result));
}

AdaptiveObjectReadSource::Window AdaptiveObjectReadSource::MakeWindow(
    std::int64_t offset, std::int64_t nominal) const {
  auto length =
      (std::min)(nominal, std::numeric_limits<std::int64_t>::max() - offset);
  if (object_size_) {
    length =
        (std::min)(length, (std::max<std::int64_t>)(*object_size_ - offset, 0));
  }
  return Window{offset, length, generation_};
}

StatusOr<ReadSourceResult> AdaptiveObjectReadSource::ReadWindows(
    char* buf, std::size_t n, ReadSourceResult result) {
  while (result.bytes_received < n && !eof_) {
    if (!source_) {
      auto status = OpenNextWindow();
      if (!status.ok()) return Fail(std::move(result), std::move(status));
      if (eof_) break;
    }
    auto const want =
        (std::min)(static_cast<std::int64_t>(n - result.bytes_received),
                   WindowEnd() - offset_);
    if (want <= 0) {
      auto status = FinishWindow(result);
      if (!status.ok()) return Fail(std::move(result), std::move(status));
      continue;
    }
    // A single read may cover the rest of the window, check the threshold
    // against the offset this read reaches.
    MaybePrefetch(offset_ + want);
    StatusOr<ReadSourceResult> data = source_->Read(
        buf + result.bytes_received, static_cast<std::size_t>(want));
    if (!data) {
      // The window source has exhausted its retry policy (or found a permanent
      // error), there is nothing left to read from.
      source_.reset();
      return Fail(std::move(result), std::move(data).status());
    }
    if (offset_ == window_.offset && IgnoresRange(*data, source_->IsOpen())) {
      source_.reset();
      if (in_first_window_ && offset_ == start_offset_) {
        return FallBackToUnbounded(buf, n, std::move(result));
      }
      return Fail(std::move(result),
                  google::cloud::internal::FailedPreconditionError(
                      absl::StrCat("the service did not honor the range "
                                   "requested for [",
                                   window_.offset, ", ", WindowEnd(),
                                   "), the object may have changed"),
                      GCP_ERROR_INFO()));
    }
    auto const received = static_cast<std::int64_t>(data->bytes_received);
    if (received > want) {
      source_.reset();
      return Fail(std::move(result),
                  google::cloud::internal::InternalError(
                      "the service returned more data than requested",
                      GCP_ERROR_INFO()));
    }
    auto const code = data->response.status_code;
    OnWindowData(result, *std::move(data));
    offset_ += received;
    if (offset_ >= WindowEnd() || !source_->IsOpen()) {
      if (code != HttpStatusCode::kContinue) result.response.status_code = code;
      auto status = FinishWindow(result);
      if (!status.ok()) return Fail(std::move(result), std::move(status));
      continue;
    }
    if (received == 0) {
      source_.reset();
      return Fail(std::move(result), google::cloud::internal::InternalError(
                                         "the download stopped making progress",
                                         GCP_ERROR_INFO()));
    }
    // The first read of the download returns the size of the object, which is
    // needed to prefetch the next window.
    MaybePrefetch(offset_);
  }
  // More windows follow: the stream is not done, whatever the status of the
  // window that just ended.
  if (!eof_) result.response.status_code = HttpStatusCode::kContinue;
  return result;
}

StatusOr<ReadSourceResult> AdaptiveObjectReadSource::FallBackToUnbounded(
    char* buf, std::size_t n, ReadSourceResult result) {
  // The service does not honor byte ranges when it transforms the object (e.g.
  // decompressive transcoding), its response contains the full object. That
  // response is not usable: if it fails, the window source resumes it with a
  // range based on the window, which the transformed data overruns. A regular
  // download resumes correctly, and no data has been returned yet, so the
  // download can switch to one.
  AbandonPrefetch();
  pass_through_ = true;
  if (!open_unbounded_) {
    return Fail(
        std::move(result),
        google::cloud::internal::FailedPreconditionError(
            "the service did not honor the range requested", GCP_ERROR_INFO()));
  }
  StatusOr<std::unique_ptr<ObjectReadSource>> source = open_unbounded_();
  if (!source) return Fail(std::move(result), std::move(source).status());
  source_ = *std::move(source);
  return ReadPassThrough(buf, n, std::move(result));
}

StatusOr<ReadSourceResult> AdaptiveObjectReadSource::ReadPassThrough(
    char* buf, std::size_t n, ReadSourceResult result) {
  while (result.bytes_received < n && source_ && source_->IsOpen()) {
    StatusOr<ReadSourceResult> data =
        source_->Read(buf + result.bytes_received, n - result.bytes_received);
    if (!data) {
      source_.reset();
      return Fail(std::move(result), std::move(data).status());
    }
    auto const received = data->bytes_received;
    result.response.status_code = data->response.status_code;
    MergeResult(result, *std::move(data));
    if (received == 0) break;
  }
  return result;
}

void AdaptiveObjectReadSource::OnWindowData(ReadSourceResult& result,
                                            ReadSourceResult data) {
  if (!generation_ && data.generation) generation_ = data.generation;
  if (!object_size_) {
    // Over REST, `data.size` falls back to `content-length` when the response
    // has no better information. That is the size of the window, not the size
    // of the object, so prefer the `content-range` header.
    auto size = SizeFromContentRange(data.response.headers);
    if (!size && data.size) size = static_cast<std::int64_t>(*data.size);
    if (size) {
      object_size_ = size;
      window_.length =
          (std::min)(window_.length,
                     (std::max<std::int64_t>)(*size - window_.offset, 0));
    }
  }
  if (in_first_window_) {
    RewriteHeaders(data.response.headers);
  } else {
    // The application already received the headers of the first window. These
    // describe a different (partial) response, and would only add duplicates.
    data.response.headers.clear();
  }
  MergeResult(result, std::move(data));
}

void AdaptiveObjectReadSource::RewriteHeaders(
    std::multimap<std::string, std::string>& headers) const {
  // Describe the download the application requested, rather than the window
  // the library requested on its behalf.
  auto const range = headers.find("content-range");
  if (range != headers.end()) {
    if (start_offset_ == 0) {
      // A download of the full object has no `content-range`.
      headers.erase(range);
    } else if (object_size_) {
      range->second = absl::StrCat("bytes ", start_offset_, "-",
                                   *object_size_ - 1, "/", *object_size_);
    }
  }
  auto const length = headers.find("content-length");
  if (length != headers.end() && object_size_) {
    length->second = std::to_string(*object_size_ - start_offset_);
  }
}

Status AdaptiveObjectReadSource::FinishWindow(ReadSourceResult& result) {
  auto source = std::move(source_);
  auto const delivered = offset_ - window_.offset;
  if (source->IsOpen()) {
    // All the data in this window has been delivered. Reading the end of the
    // response completes the request, so the connection can be reused. Errors
    // here cannot lose any data, and are ignored.
    auto metadata = Discard(*source, 0);
    // A gRPC response returns its headers with its last read.
    if (metadata) {
      if (metadata->response.status_code != HttpStatusCode::kContinue) {
        result.response.status_code = metadata->response.status_code;
      }
      OnWindowData(result, *std::move(metadata));
    }
  }
  source.reset();
  in_first_window_ = false;
  if (object_size_ && offset_ >= *object_size_) {
    eof_ = true;
    return Status{};
  }
  if (delivered >= window_.length) return Status{};
  // The window ended before its last byte. Without a size, that is how the end
  // of the object is found.
  if (!object_size_) {
    eof_ = true;
    return Status{};
  }
  // The object has more data, the next window starts at the current offset. A
  // window that ends without any data would repeat forever.
  if (delivered == 0) {
    return google::cloud::internal::InternalError(
        absl::StrCat("the download of [", window_.offset, ", ", WindowEnd(),
                     ") ended without any data, the object has ", *object_size_,
                     " bytes"),
        GCP_ERROR_INFO());
  }
  return Status{};
}

Status AdaptiveObjectReadSource::OpenNextWindow() {
  auto const nominal = NextAdaptiveWindowSize(window_nominal_, settings_);
  auto const window = MakeWindow(offset_, nominal);
  StatusOr<std::unique_ptr<ObjectReadSource>> source = [&] {
    auto prefetched = TakePrefetch(window);
    if (prefetched) return *std::move(prefetched);
    return (*factory_)(window);
  }();
  if (!source) {
    // Without a size, the end of an object whose size is a multiple of the
    // window boundaries is only found by reading past it.
    if (!object_size_ && source.status().code() == StatusCode::kOutOfRange) {
      eof_ = true;
      return Status{};
    }
    return std::move(source).status();
  }
  source_ = *std::move(source);
  window_ = window;
  window_nominal_ = nominal;
  return Status{};
}

void AdaptiveObjectReadSource::MaybePrefetch(std::int64_t read_end) {
  if (prefetch_ || !executor_ || !PrefetchEnabled(settings_)) return;
  // Without a size the next window may be past the end of the object.
  if (!object_size_ || WindowEnd() >= *object_size_) return;
  auto const consumed = read_end - window_.offset;
  if (static_cast<double>(consumed) <
      settings_.prefetch_threshold * static_cast<double>(window_.length)) {
    return;
  }
  auto state = std::make_shared<PrefetchState>(MakeWindow(
      WindowEnd(), NextAdaptiveWindowSize(window_nominal_, settings_)));
  auto task = [state, factory = std::weak_ptr<WindowFactory const>(factory_)] {
    RunPrefetch(state, factory);
  };
  // If the task cannot be scheduled the window is opened when needed.
  if (!executor_(std::move(task))) return;
  prefetch_ = std::move(state);
}

void AdaptiveObjectReadSource::RunPrefetch(
    std::shared_ptr<PrefetchState> const& state,
    std::weak_ptr<WindowFactory const> const& factory) {
  {
    std::lock_guard<std::mutex> lk(state->mu);
    if (state->abandoned) return;
    state->started = true;
  }
  StatusOr<std::unique_ptr<ObjectReadSource>> source =
      [&]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    // Hold the factory only while opening the window. It may own resources,
    // such as the thread pool running this task.
    auto f = factory.lock();
    if (!f) {
      return google::cloud::internal::CancelledError(
          "the download was closed before the prefetch started",
          GCP_ERROR_INFO());
    }
    return (*f)(state->window);
  }();
  std::unique_lock<std::mutex> lk(state->mu);
  // An abandoned window is released when `source` goes out of scope, after
  // the lock is released.
  if (state->abandoned) return;
  state->source = std::move(source);
  state->done = true;
  lk.unlock();
  state->cv.notify_all();
}

std::optional<StatusOr<std::unique_ptr<ObjectReadSource>>>
AdaptiveObjectReadSource::TakePrefetch(Window const& window) {
  if (!prefetch_) return std::nullopt;
  auto state = std::move(prefetch_);
  std::unique_lock<std::mutex> lk(state->mu);
  auto const matches = state->window.offset == window.offset &&
                       state->window.length == window.length &&
                       state->window.generation == window.generation;
  if (!matches || !state->started) {
    // A prefetch that has not started is still waiting for a thread, opening
    // the window directly is faster. A prefetch for a different window (e.g.
    // after a window ended early) is of no use.
    state->abandoned = true;
    auto discarded = std::move(state->source);
    lk.unlock();
    return std::nullopt;
  }
  state->cv.wait(lk, [&state] { return state->done; });
  return std::move(state->source);
}

void AdaptiveObjectReadSource::AbandonPrefetch() {
  if (!prefetch_) return;
  auto state = std::move(prefetch_);
  StatusOr<std::unique_ptr<ObjectReadSource>> discarded;
  {
    std::lock_guard<std::mutex> lk(state->mu);
    state->abandoned = true;
    discarded = std::move(state->source);
  }
  // Any prefetched window is released here, outside the lock.
}

StatusOr<ReadSourceResult> AdaptiveObjectReadSource::Fail(
    ReadSourceResult result, Status status) {
  if (result.bytes_received != 0) {
    // Return the data received so far, and the error in the next call.
    pending_error_ = std::move(status);
    result.response.status_code = HttpStatusCode::kContinue;
    return result;
  }
  // The download cannot continue, release its resources.
  closed_ = true;
  AbandonPrefetch();
  source_.reset();
  return status;
}

StatusOr<std::unique_ptr<ObjectReadSource>> MakeAdaptiveObjectReadSource(
    std::shared_ptr<AdaptiveObjectReadSource::WindowFactory const> factory,
    AdaptiveObjectReadSource::UnboundedFactory open_unbounded,
    AdaptiveObjectReadSource::Executor executor, AdaptiveReadSettings settings,
    std::int64_t offset, std::optional<std::int64_t> generation) {
  AdaptiveObjectReadSource::Window first{
      offset,
      (std::min)(settings.initial_window,
                 std::numeric_limits<std::int64_t>::max() - offset),
      generation};
  auto source = (*factory)(first);
  if (!source) {
    // The service rejects every range of an empty object. A download without
    // a range returns the (empty) object.
    if (offset == 0 && open_unbounded &&
        source.status().code() == StatusCode::kOutOfRange) {
      return open_unbounded();
    }
    return source;
  }
  return std::unique_ptr<ObjectReadSource>(
      std::make_unique<AdaptiveObjectReadSource>(
          std::move(factory), std::move(open_unbounded), std::move(executor),
          settings, std::move(first), *std::move(source)));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
