// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_MULTI_STREAM_MANAGER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_MULTI_STREAM_MANAGER_H

#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <unordered_map>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Defines the interface contract that any stream type (e.g., ReadStream,
// WriteStream) managed by MultiStreamManager must implement. This explicit base
// class ensures we have a clear, enforceable interface for operations like
// CancelAll().
class StreamBase {
 public:
  virtual ~StreamBase() = default;
  virtual void Cancel() = 0;
};

// Manages a collection of streams.
//
// This class implements the "Subsequent Stream" logic where idle streams
// are moved to the front of the queue for reuse.
//
// THREAD SAFETY:
// This class is NOT thread-safe. The owner (e.g. ObjectDescriptorImpl
// or AsyncWriterImpl etc) must serialize access, typically by holding
// an external mutex while calling these methods.
//
// EXAMPLE USAGE:
//   class MyOwner {
//     std::mutex mu_;
//     MultiStreamManager<MyStream, MyRange> manager_;
//
//     void StartRead() {
//        std::unique_lock<std::mutex> lk(mu_);
//        auto it = manager_.GetLeastBusyStream();
//     }
//   };
template <typename StreamT, typename RangeT>
class MultiStreamManager {
 public:
  struct Stream {
    std::shared_ptr<StreamT> stream;
    std::unordered_map<std::int64_t, std::shared_ptr<RangeT>> active_ranges;
  };

  using StreamIterator = typename std::list<Stream>::iterator;
  using StreamFactory = std::function<std::shared_ptr<StreamT>()>;

  // Constructor creates the first stream using the factory immediately.
  explicit MultiStreamManager(StreamFactory stream_factory)
      : stream_factory_(std::move(stream_factory)) {
    streams_.push_back(Stream{stream_factory_(), {}});
  }

  // Constructor accepts an already-created initial stream.
  // This is required by ObjectDescriptorImpl which receives an OpenStream.
  MultiStreamManager(StreamFactory stream_factory,
                     std::shared_ptr<StreamT> initial_stream)
      : stream_factory_(std::move(stream_factory)) {
    streams_.push_back(Stream{std::move(initial_stream), {}});
  }

  StreamIterator GetFirstStream() {
    if (streams_.empty()) return streams_.end();
    return streams_.begin();
  }

  StreamIterator GetLeastBusyStream() {
    if (streams_.empty()) return streams_.end();
    auto least_busy_it = streams_.begin();
    // Track min_ranges to avoid calling .size() repeatedly if possible,
    // though for std::unordered_map .size() is O(1).
    std::size_t min_ranges = least_busy_it->active_ranges.size();
    if (min_ranges == 0) return least_busy_it;

    // Start checking from the second element
    for (auto it = std::next(streams_.begin()); it != streams_.end(); ++it) {
      // Strict less-than ensures stability (preferring older streams if tied)
      auto size = it->active_ranges.size();
      if (size < min_ranges) {
        least_busy_it = it;
        min_ranges = size;
        if (min_ranges == 0) return least_busy_it;
      }
    }
    return least_busy_it;
  }

  StreamIterator AddStream(std::shared_ptr<StreamT> stream) {
    streams_.push_front(Stream{std::move(stream), {}});
    return streams_.begin();
  }

  void CancelAll() {
    for (auto& s : streams_) {
      if (s.stream) s.stream->Cancel();
    }
  }

  void RemoveStreamAndNotifyRanges(StreamIterator it, Status const& status) {
    auto ranges = std::move(it->active_ranges);
    streams_.erase(it);
    for (auto const& kv : ranges) {
      kv.second->OnFinish(status);
    }
  }

  void MoveActiveRanges(StreamIterator from, StreamIterator to) {
    to->active_ranges = std::move(from->active_ranges);
  }

  void CleanupDoneRanges(StreamIterator it) {
    auto& active_ranges = it->active_ranges;
    for (auto i = active_ranges.begin(); i != active_ranges.end();) {
      if (i->second->IsDone()) {
        i = active_ranges.erase(i);
      } else {
        ++i;
      }
    }
  }

  template <typename Pred>
  bool ReuseIdleStreamToFront(Pred pred) {
    for (auto it = streams_.begin(); it != streams_.end(); ++it) {
      if (!pred(*it)) continue;

      // If the idle stream is already at the front, we don't
      // need to move it. Otherwise splice to the front in O(1).
      if (it != streams_.begin()) {
        streams_.splice(streams_.begin(), streams_, it);
      }
      return true;
    }
    return false;
  }

  bool Empty() const { return streams_.empty(); }
  StreamIterator End() { return streams_.end(); }
  std::size_t Size() const { return streams_.size(); }

 private:
  std::list<Stream> streams_;
  StreamFactory stream_factory_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_MULTI_STREAM_MANAGER_H
