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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ADAPTIVE_OBJECT_READ_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ADAPTIVE_OBJECT_READ_SOURCE_H

#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/internal/object_read_source.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Marks the requests issued by an adaptive read.
 *
 * `StorageConnectionImpl` sets this option on the requests that open the
 * windows of an adaptive read. The REST stub then adds
 * `gccl-feat/adaptive-read` to the `x-goog-api-client` header, which separates
 * these requests from other downloads in the service logs.
 */
struct AdaptiveReadAttributionOption {
  using Type = bool;
};

/**
 * Returns the `x-goog-api-client` header value for a download using
 * @p options.
 *
 * This is the usual value of the header, with `gccl-feat/adaptive-read`
 * appended when `AdaptiveReadAttributionOption` is set.
 */
std::string AdaptiveReadApiClientHeader(Options const& options);

/// The windowing parameters of an adaptive read, after validation.
struct AdaptiveReadSettings {
  std::int64_t initial_window;
  std::int64_t max_window;
  double growth_factor;
  double prefetch_threshold;
};

/// Validates @p config, replacing invalid values with usable ones.
AdaptiveReadSettings MakeAdaptiveReadSettings(
    storage_experimental::AdaptiveReadRangeConfig const& config);

/// Returns the size of the window after a window of @p current bytes.
std::int64_t NextAdaptiveWindowSize(std::int64_t current,
                                    AdaptiveReadSettings const& settings);

/**
 * Reads an open-ended range of an object as a series of bounded windows.
 *
 * A download that reads to the end of the object (the full object, or from an
 * offset) is served by a sequence of downloads with bounded ranges. The first
 * window covers `initial_window` bytes, each window after that is
 * `growth_factor` times larger, up to `max_window` bytes. An application that
 * closes the stream early leaves at most one window outstanding, instead of an
 * unbounded download whose data the service sends only to be discarded.
 *
 * Each window is created by a `WindowFactory`, typically as a
 * `RetryObjectReadSource`, so a failure resumes inside the current window.
 * This class adds:
 *
 * - A single, continuous stream: `Read()` fills the caller's buffer across
 *   window boundaries, and the stream only reports that it is closed when the
 *   last byte of the object has been delivered.
 * - Generation pinning: every window after the first reads the generation
 *   returned by the first window.
 * - Faithful metadata: the headers of the first window are rewritten to
 *   describe the full download (`content-length`, `content-range`), and the
 *   headers of later windows are not reported again. The full-object hashes
 *   reported by the service are passed through, so `ObjectReadStreambuf` can
 *   validate checksums computed across all the windows.
 * - Decompressive transcoding: the service ignores byte ranges when it
 *   decompresses an object on the fly, and returns the full object. If the
 *   first window shows that the service ignored its range, the source
 *   discards that response and switches to a regular download (created by an
 *   `UnboundedFactory`), which also resumes correctly after a failure.
 * - Prefetching: once a read reaches `prefetch_threshold` of a window, the next
 *   window is opened in the background using the `Executor`. The threshold is
 *   checked before each read, so a single large read that spans several
 *   windows also prefetches them. If the prefetch has not started by the time
 *   the application needs the window, the window is opened directly instead.
 * - Bounded drain on close: if the application closes the stream with at most
 *   `initial_window` bytes left in the current window, those bytes are read
 *   and discarded. The response completes normally, and the connection can be
 *   reused.
 */
class AdaptiveObjectReadSource : public ObjectReadSource {
 public:
  /// The byte range `[offset, offset + length)` of a single window.
  struct Window {
    std::int64_t offset = 0;
    std::int64_t length = 0;
    std::optional<std::int64_t> generation;
  };

  /**
   * Opens a source for a single window.
   *
   * This function may be called from a background thread, to prefetch the
   * next window.
   */
  using WindowFactory =
      std::function<StatusOr<std::unique_ptr<ObjectReadSource>>(Window const&)>;

  /**
   * Opens a regular download of the data requested by the application.
   *
   * That is, a download of the full object, or from the start offset, without
   * windows. It is only called from the thread calling `Read()`.
   */
  using UnboundedFactory =
      std::function<StatusOr<std::unique_ptr<ObjectReadSource>>()>;

  /// Runs a task in the background. Returns false if the task cannot run.
  using Executor = std::function<bool(std::function<void()>)>;

  /**
   * Creates a source whose first window is already open.
   *
   * @param factory creates the windows after @p first_window.
   * @param open_unbounded creates a regular download, used if the service
   *     does not honor the range of the first window. If empty, the download
   *     fails in that case.
   * @param executor runs prefetch tasks, may be empty to disable prefetching.
   * @param settings the windowing parameters.
   * @param first_window the first window, its offset is the start of the
   *     download.
   * @param first_source the source reading @p first_window.
   */
  AdaptiveObjectReadSource(std::shared_ptr<WindowFactory const> factory,
                           UnboundedFactory open_unbounded, Executor executor,
                           AdaptiveReadSettings settings, Window first_window,
                           std::unique_ptr<ObjectReadSource> first_source);
  ~AdaptiveObjectReadSource() override;

  AdaptiveObjectReadSource(AdaptiveObjectReadSource const&) = delete;
  AdaptiveObjectReadSource& operator=(AdaptiveObjectReadSource const&) = delete;
  AdaptiveObjectReadSource(AdaptiveObjectReadSource&&) = delete;
  AdaptiveObjectReadSource& operator=(AdaptiveObjectReadSource&&) = delete;

  bool IsOpen() const override;
  StatusOr<HttpResponse> Close() override;
  StatusOr<ReadSourceResult> Read(char* buf, std::size_t n) override;

 private:
  struct PrefetchState;

  static void RunPrefetch(std::shared_ptr<PrefetchState> const& state,
                          std::weak_ptr<WindowFactory const> const& factory);

  std::int64_t WindowEnd() const { return window_.offset + window_.length; }
  Window MakeWindow(std::int64_t offset, std::int64_t nominal) const;

  StatusOr<ReadSourceResult> ReadWindows(char* buf, std::size_t n,
                                         ReadSourceResult result);
  StatusOr<ReadSourceResult> FallBackToUnbounded(char* buf, std::size_t n,
                                                 ReadSourceResult result);
  StatusOr<ReadSourceResult> ReadPassThrough(char* buf, std::size_t n,
                                             ReadSourceResult result);
  void OnWindowData(ReadSourceResult& result, ReadSourceResult data);
  void RewriteHeaders(std::multimap<std::string, std::string>& headers) const;
  Status FinishWindow(ReadSourceResult& result);
  Status OpenNextWindow();
  // Prefetches the next window if the application reads the current window up
  // to (but excluding) @p read_end.
  void MaybePrefetch(std::int64_t read_end);
  std::optional<StatusOr<std::unique_ptr<ObjectReadSource>>> TakePrefetch(
      Window const& window);
  void AbandonPrefetch();
  StatusOr<ReadSourceResult> Fail(ReadSourceResult result, Status status);

  std::shared_ptr<WindowFactory const> factory_;
  UnboundedFactory open_unbounded_;
  Executor executor_;
  AdaptiveReadSettings settings_;
  std::int64_t start_offset_;
  // The offset of the next byte returned to the caller.
  std::int64_t offset_;
  std::optional<std::int64_t> generation_;
  std::optional<std::int64_t> object_size_;
  // The current window (or the last window, between windows), and its nominal
  // size. The nominal size drives the growth of the windows, `window_.length`
  // may be smaller near the end of the object.
  Window window_;
  std::int64_t window_nominal_;
  // Reads the current window, or the regular download in pass-through mode.
  // Null between windows.
  std::unique_ptr<ObjectReadSource> source_;
  bool in_first_window_ = true;
  // The source switched to a regular download, see `FallBackToUnbounded()`.
  bool pass_through_ = false;
  bool eof_ = false;
  bool closed_ = false;
  // An error found after some data was returned, reported on the next call.
  Status pending_error_;
  std::shared_ptr<PrefetchState> prefetch_;
};

/**
 * Starts an adaptive read at @p offset.
 *
 * The first window is opened before this function returns, so errors such as
 * `kNotFound` are reported here, as they are for a regular download.
 *
 * The service rejects any range for an empty object. If the first window of a
 * full object download (@p offset is 0) fails with `kOutOfRange`, this function
 * returns the source created by @p open_unbounded, a regular download of the
 * full object.
 */
StatusOr<std::unique_ptr<ObjectReadSource>> MakeAdaptiveObjectReadSource(
    std::shared_ptr<AdaptiveObjectReadSource::WindowFactory const> factory,
    AdaptiveObjectReadSource::UnboundedFactory open_unbounded,
    AdaptiveObjectReadSource::Executor executor, AdaptiveReadSettings settings,
    std::int64_t offset, std::optional<std::int64_t> generation);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ADAPTIVE_OBJECT_READ_SOURCE_H
