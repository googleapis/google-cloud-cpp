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
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/internal/async/checksum_helpers.h"
#include "google/cloud/storage/internal/async/handle_redirect_error.h"
#include "google/cloud/storage/internal/async/multi_stream_manager.h"
#include "google/cloud/storage/internal/async/object_descriptor_reader_tracing.h"
#include "google/cloud/storage/internal/async/options.h"
#include "google/cloud/storage/internal/async/read_range.h"
#include "google/cloud/storage/internal/grpc/object_metadata_parser.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/hash_validator_impl.h"
#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/rpc/status.pb.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

enum class InitialReadRangesCacheStatus {
  kNone,
  kMiss,
  kHit,
  kEvicted,
};

absl::string_view CacheStatusToString(InitialReadRangesCacheStatus status) {
  switch (status) {
    case InitialReadRangesCacheStatus::kHit:
      return "HIT";
    case InitialReadRangesCacheStatus::kEvicted:
      return "EVICTED";
    case InitialReadRangesCacheStatus::kMiss:
      return "MISS";
    case InitialReadRangesCacheStatus::kNone:
      return "";
  }
  return "INVALID_STATUS";
}

}  // namespace

ObjectDescriptorImpl::ObjectDescriptorImpl(
    std::unique_ptr<storage::ResumePolicy> resume_policy,
    OpenStreamFactory make_stream,
    google::storage::v2::BidiReadObjectSpec read_object_spec,
    std::shared_ptr<OpenStream> stream, Options options,
    std::function<bool()> transport_ok)
    : resume_policy_prototype_(std::move(resume_policy)),
      make_stream_(std::move(make_stream)),
      read_object_spec_(std::move(read_object_spec)),
      options_(std::move(options)),
      has_initial_read_ranges_(options_.has<ReadRangesOption>()),
      transport_ok_(std::move(transport_ok)) {
  auto initial_read_stream = std::make_shared<ReadStream>(
      std::move(stream), resume_policy_prototype_->clone());
  // Notify the resume policy that the initial stream was established
  // successfully.
  initial_read_stream->resume_policy->OnStartSuccess();
  stream_manager_ = std::make_unique<StreamManager>(
      []() -> std::shared_ptr<ReadStream> { return nullptr; },  // NOLINT
      std::move(initial_read_stream));
  // Initialize the pacing limit from options if configured.
  if (options_.has<PreWarmBufferLimitOption>()) {
    max_prewarmed_buffer_size_ = options_.get<PreWarmBufferLimitOption>();
  }
  // If pre-warmed ranges are specified, initialize their `ReadRange` objects,
  // register them as active on the initial stream, and cache them.
  if (options_.has<ReadRangesOption>()) {
    auto const& ranges = options_.get<ReadRangesOption>();
    auto it = stream_manager_->GetFirstStream();
    if (it != stream_manager_->End()) {
      auto deduped_ranges = DeduplicateRanges(ranges);
      for (auto const& dr : deduped_ranges) {
        auto range_key = std::make_pair(dr.config.offset, dr.config.length);
        auto range = std::make_shared<ReadRange>(
            dr.config.offset, dr.config.length, read_object_spec_.bucket(),
            read_object_spec_.object());
        // Registering on the stream allows `OnRead` to route incoming data to
        // these ranges.
        it->active_ranges.emplace(dr.read_id, range);
        // Cache them so subsequent `Read()` calls can claim them.
        auto [cache_it, inserted] = prewarmed_ranges_.emplace(
            range_key, PrewarmedRange{range, dr.read_id});
        // Mark them as unclaimed for pacing checks and store the iterator.
        unclaimed_ranges_.emplace(dr.read_id, UnclaimedRangeState{0, cache_it});
      }
      // Ensure new dynamically requested ranges use IDs that don't conflict
      // with pre-warmed ones.
      if (!deduped_ranges.empty()) {
        read_id_generator_ = deduped_ranges.back().read_id;
      }
    }
  }
}

ObjectDescriptorImpl::~ObjectDescriptorImpl() { Cancel(); }

void ObjectDescriptorImpl::Start(
    google::storage::v2::BidiReadObjectResponse first_response) {
  std::unique_lock<std::mutex> lk(mu_);
  auto it = stream_manager_->GetFirstStream();
  if (it == stream_manager_->End()) return;
  std::shared_ptr<ReadStream> read_stream = it->stream;
  std::shared_ptr<OpenStream> current_stream = read_stream->stream;
  lk.unlock();
  OnRead(read_stream, current_stream, std::move(first_response));
  // Acquire lock and queue the background stream if multi-stream optimization
  // is enabled.
  if (options_.get<storage::EnableMultiStreamOptimizationOption>()) {
    lk.lock();
    AssurePendingStreamQueued(lk);
  }
}

bool ObjectDescriptorImpl::IsOpen() const {
  {
    std::scoped_lock<std::mutex> lk(mu_);
    if (cancelled_) return false;
    if (stream_manager_->Empty()) return false;
  }
  return !transport_ok_ || transport_ok_();
}

void ObjectDescriptorImpl::Cancel() {
  std::unique_lock<std::mutex> lk(mu_);
  if (cancelled_) return;
  cancelled_ = true;
  if (stream_manager_) stream_manager_->CancelAll();
  if (pending_stream_.valid()) pending_stream_.cancel();
}

std::optional<google::storage::v2::Object> ObjectDescriptorImpl::metadata()
    const {
  std::unique_lock<std::mutex> lk(mu_);
  return metadata_;
}

void ObjectDescriptorImpl::AssurePendingStreamQueued(
    std::unique_lock<std::mutex> const&) {
  if (pending_stream_.valid()) return;
  auto request = google::storage::v2::BidiReadObjectRequest{};

  *request.mutable_read_object_spec() = read_object_spec_;
  pending_stream_ = make_stream_(std::move(request));
}

void ObjectDescriptorImpl::MakeSubsequentStream() {
  if (!options_.get<storage::EnableMultiStreamOptimizationOption>()) {
    // Do nothing if multi-stream optimization is disabled.
    return;
  }

  std::unique_lock<std::mutex> lk(mu_);
  // Reuse an idle stream if possible.
  if (stream_manager_->ReuseIdleStreamToFront(
          [](StreamManager::Stream const& s) {
            auto const* rs = s.stream.get();
            return rs != nullptr && s.active_ranges.empty() &&
                   !rs->write_pending;
          })) {
    return;
  }
  // Proactively create a new stream if needed.
  AssurePendingStreamQueued(lk);
  if (!pending_stream_.valid()) return;
  auto stream_future = std::move(pending_stream_);
  lk.unlock();

  // Use .then() to retrieves the result without blocking.
  stream_future.then([w = WeakFromThis()](auto f) {
    auto self = w.lock();
    if (!self) return;

    StatusOr<OpenStreamResult> stream_result = f.get();
    if (!stream_result) {
      // Stream creation failed.
      // The next call to AssurePendingStreamQueued will retry creation.
      return;
    }

    std::unique_lock<std::mutex> lk(self->mu_);
    if (self->cancelled_) return;

    std::shared_ptr<ReadStream> read_stream =
        std::make_shared<ReadStream>(std::move(stream_result->stream),
                                     self->resume_policy_prototype_->clone());
    read_stream->resume_policy->OnStartSuccess();

    self->stream_manager_->AddStream(read_stream);

    // Now that we consumed pending_stream_, queue the next one immediately.
    self->AssurePendingStreamQueued(lk);

    std::shared_ptr<OpenStream> new_stream = read_stream->stream;
    lk.unlock();
    self->OnRead(read_stream, new_stream,
                 std::move(stream_result->first_response));
  });
}

std::unique_ptr<storage::AsyncReaderConnection> ObjectDescriptorImpl::Read(
    ReadParams p) {
  // Full-object checksum validation (both CRC32C and MD5) is only supported for
  // full-object reads (starting at offset 0 and reading the entire object) of
  // finalized objects.
  //
  // For unfinalized objects, full-object checksum validation is bypassed
  // because the object size and checksums can dynamically change as data is
  // appended, while chunk-level CRC32C validation continues to protect data on
  // the wire.
  //
  // Note that MD5 validation is not supported for partial/ranged reads because
  // GCS does not compute or send chunk-level MD5 checksums (unlike CRC32C,
  // which is validated per-chunk on the gRPC layer).
  bool const is_finalized =
      metadata_.has_value() && metadata_->has_finalize_time();
  bool const is_full_read = is_finalized && (p.start == 0) &&
                            (p.length == 0 || p.length >= metadata_->size());

  auto hash_function = CreateHashFunction(is_full_read);
  auto hash_validator = CreateHashValidator(is_full_read);

  // Calculate the download limit (number of bytes to read) based on GCS range
  // read options:
  // 1. If `p.start < 0`, this is a tail read (ReadLast). The limit is the
  // absolute value of `p.start`.
  //    We handle the overflow edge case where `p.start` is the minimum signed
  //    value.
  // 2. If `p.start >= 0` and `p.length > 0`, this is a standard range request
  // (ReadRange). The limit is `p.length`.
  // 3. Otherwise, the limit is not set (unlimited / read to end).
  std::optional<std::int64_t> limit;
  if (p.start < 0) {
    if (p.start == (std::numeric_limits<std::int64_t>::min)()) {
      limit = (std::numeric_limits<std::int64_t>::max)();
    } else {
      limit = -p.start;
    }
  } else if (p.length > 0) {
    limit = p.length;
  }

  auto range = std::make_shared<ReadRange>(
      p.start, limit, hash_function, std::move(hash_validator),
      read_object_spec_.bucket(), read_object_spec_.object());

  std::unique_lock<std::mutex> lk(mu_);
  // Check if this range matches a pre-warmed range.
  auto cache_key = std::make_pair(p.start, p.length);
  auto cache_it = prewarmed_ranges_.find(cache_key);
  auto cache_status = has_initial_read_ranges_
                          ? InitialReadRangesCacheStatus::kMiss
                          : InitialReadRangesCacheStatus::kNone;

  if (cache_it != prewarmed_ranges_.end()) {
    cache_status = InitialReadRangesCacheStatus::kHit;
    // Cache hit. Claim the pre-warmed range and return it to the user.
    auto prewarmed = std::move(cache_it->second);
    prewarmed_ranges_.erase(cache_it);
    // Mark as claimed so we stop applying pacing constraints to it, and
    // reclaim the buffer budget used by this range atomically under the lock.
    auto unclaimed_it = unclaimed_ranges_.find(prewarmed.read_id);
    if (unclaimed_it != unclaimed_ranges_.end()) {
      total_prewarmed_bytes_buffered_ -= unclaimed_it->second.bytes_buffered;
      unclaimed_ranges_.erase(unclaimed_it);
    }
    lk.unlock();
    if (!internal::TracingEnabled(options_)) {
      return std::unique_ptr<storage::AsyncReaderConnection>(
          std::make_unique<ObjectDescriptorReader>(std::move(prewarmed.range)));
    }
    return MakeTracingObjectDescriptorReader(std::move(prewarmed.range),
                                             CacheStatusToString(cache_status));
  }

  // If not hit, check if it was evicted earlier due to pacing.
  if (evicted_ranges_.erase(cache_key) != 0) {
    cache_status = InitialReadRangesCacheStatus::kEvicted;
  }

  if (stream_manager_->Empty()) {
    lk.unlock();
    range->OnFinish(Status(StatusCode::kFailedPrecondition,
                           "Cannot read object, all streams failed"));
    if (!internal::TracingEnabled(options_)) {
      return std::unique_ptr<storage::AsyncReaderConnection>(
          std::make_unique<ObjectDescriptorReader>(std::move(range)));
    }
    return MakeTracingObjectDescriptorReader(std::move(range),
                                             CacheStatusToString(cache_status));
  }

  auto it = stream_manager_->GetLeastBusyStream();
  std::shared_ptr<ReadStream> read_stream = it->stream;
  std::int64_t const id = ++read_id_generator_;
  it->active_ranges.emplace(id, range);
  google::storage::v2::ReadRange& read_range =
      *read_stream->next_request.add_read_ranges();
  read_range.set_read_id(id);
  read_range.set_read_offset(p.start);
  read_range.set_read_length(p.length);
  Flush(std::move(lk), read_stream);

  if (!internal::TracingEnabled(options_)) {
    return std::unique_ptr<storage::AsyncReaderConnection>(
        std::make_unique<ObjectDescriptorReader>(std::move(range)));
  }
  return MakeTracingObjectDescriptorReader(std::move(range),
                                           CacheStatusToString(cache_status));
}

std::shared_ptr<storage::internal::HashFunction>
ObjectDescriptorImpl::CreateHashFunction(bool is_full_read) const {
  auto const settings = GetDownloadChecksumSettings(options_);
  auto const enable_crc32c = settings.enable_crc32c;
  auto const enable_md5 = settings.enable_md5;

  if (enable_crc32c) {
    std::unique_ptr<storage::internal::HashFunction> child;
    if (is_full_read) {
      if (enable_md5) {
        child = std::make_unique<storage::internal::CompositeFunction>(
            std::make_unique<storage::internal::Crc32cHashFunction>(),
            storage::internal::MD5HashFunction::Create());
      } else {
        child = std::make_unique<storage::internal::Crc32cHashFunction>();
      }
    } else {
      child = storage::internal::CreateNullHashFunction();
    }
    return std::make_shared<storage::internal::Crc32cMessageHashFunction>(
        std::move(child));
  }
  if (enable_md5 && is_full_read) {
    return std::shared_ptr<storage::internal::HashFunction>(
        storage::internal::MD5HashFunction::Create());
  }
  return std::shared_ptr<storage::internal::HashFunction>(
      storage::internal::CreateNullHashFunction());
}

std::unique_ptr<storage::internal::HashValidator>
ObjectDescriptorImpl::CreateHashValidator(bool is_full_read) const {
  if (!is_full_read) {
    return storage::internal::CreateNullHashValidator();
  }

  auto const settings = GetDownloadChecksumSettings(options_);
  auto const enable_crc32c = settings.enable_crc32c;
  auto const enable_md5 = settings.enable_md5;

  std::unique_ptr<storage::internal::HashValidator> hash_validator;
  if (enable_crc32c && enable_md5) {
    hash_validator = std::make_unique<storage::internal::CompositeValidator>(
        std::make_unique<storage::internal::Crc32cHashValidator>(),
        std::make_unique<storage::internal::MD5HashValidator>());
  } else if (enable_crc32c) {
    hash_validator = std::make_unique<storage::internal::Crc32cHashValidator>();
  } else if (enable_md5) {
    hash_validator = std::make_unique<storage::internal::MD5HashValidator>();
  } else {
    return storage::internal::CreateNullHashValidator();
  }

  // Process the expected hashes from metadata
  storage::internal::HashValues hashes;
  if (metadata_->has_checksums()) {
    auto const& checksums = metadata_->checksums();
    if (checksums.has_crc32c()) {
      hashes =
          Merge(std::move(hashes),
                storage::internal::HashValues{
                    storage_internal::Crc32cFromProto(checksums.crc32c()), {}});
    }
    if (!checksums.md5_hash().empty()) {
      hashes =
          Merge(std::move(hashes),
                storage::internal::HashValues{
                    {}, storage_internal::MD5FromProto(checksums.md5_hash())});
    }
  }
  hash_validator->ProcessHashValues(hashes);
  return hash_validator;
}

void ObjectDescriptorImpl::Flush(
    std::unique_lock<std::mutex> lk,
    std::shared_ptr<ReadStream> const& read_stream) {
  if (!read_stream || read_stream->resuming || read_stream->write_pending ||
      read_stream->next_request.read_ranges().empty()) {
    return;
  }
  read_stream->write_pending = true;
  google::storage::v2::BidiReadObjectRequest request;
  request.Swap(&read_stream->next_request);

  // Assign CurrentStream to a temporary variable to prevent
  // lifetime extension which can cause the lock to be held until the
  // end of the block.
  std::shared_ptr<OpenStream> current_stream = read_stream->stream;
  lk.unlock();
  current_stream->Write(std::move(request))
      .then([w = WeakFromThis(), read_stream, current_stream](auto f) {
        if (auto self = w.lock()) {
          self->OnWrite(read_stream, current_stream, f.get());
        }
      });
}

void ObjectDescriptorImpl::OnWrite(
    std::shared_ptr<ReadStream> const& read_stream,
    std::shared_ptr<OpenStream> const& stream, bool ok) {
  std::unique_lock<std::mutex> lk(mu_);
  // Discard callbacks from stale or removed streams (e.g. if the stream was
  // replaced during reconnection or removed after an error).
  auto it = stream_manager_->Find(read_stream);
  if (it == stream_manager_->End() || !it->stream ||
      it->stream->stream != stream) {
    return;
  }
  if (!ok) return DoFinish(std::move(lk), read_stream, stream);
  it->stream->write_pending = false;
  Flush(std::move(lk), read_stream);
}

void ObjectDescriptorImpl::DoRead(
    std::unique_lock<std::mutex> lk,
    std::shared_ptr<ReadStream> const& read_stream) {
  if (!read_stream || read_stream->read_pending) return;
  read_stream->read_pending = true;

  // Assign CurrentStream to a temporary variable to prevent
  // lifetime extension which can cause the lock to be held until the
  // end of the block.
  std::shared_ptr<OpenStream> current_stream = read_stream->stream;
  lk.unlock();
  current_stream->Read().then(
      [w = WeakFromThis(), read_stream, current_stream](auto f) {
        if (auto self = w.lock()) {
          self->OnRead(read_stream, current_stream, f.get());
        }
      });
}

void ObjectDescriptorImpl::OnRead(
    std::shared_ptr<ReadStream> const& read_stream,
    std::shared_ptr<OpenStream> const& stream,
    std::optional<google::storage::v2::BidiReadObjectResponse> response) {
  std::unique_lock<std::mutex> lk(mu_);
  // Discard callbacks from stale or removed streams (e.g. if the stream was
  // replaced during reconnection or removed after an error).
  auto it = stream_manager_->Find(read_stream);
  if (it == stream_manager_->End() || !it->stream ||
      it->stream->stream != stream) {
    return;
  }
  it->stream->read_pending = false;

  if (!response) return DoFinish(std::move(lk), read_stream, stream);
  if (response->has_metadata()) {
    metadata_ = std::move(*response->mutable_metadata());
  }
  if (response->has_read_handle()) {
    *read_object_spec_.mutable_read_handle() =
        std::move(*response->mutable_read_handle());
  }
  auto copy = it->active_ranges;
  bool is_transcoded = false;
  std::optional<std::int64_t> object_size;
  if (metadata_.has_value()) {
    is_transcoded = metadata_->content_encoding() == "gzip";
    object_size = metadata_->size();
  }
  // Release the lock while notifying the ranges. The notifications may trigger
  // application code, and that code may callback on this class.
  lk.unlock();

  for (auto& range_data : *response->mutable_object_data_ranges()) {
    std::int64_t id = range_data.read_range().read_id();
    auto const l = copy.find(id);
    if (l == copy.end()) continue;

    auto range = l->second;
    std::size_t chunk_size = range_data.checksummed_data().content().size();

    bool evict = false;
    lk.lock();
    // Verify the range is still active under the lock. Because `OnRead`
    // processes chunks in batches, an earlier chunk in the same batch could
    // breach the pacing limit and evict a subsequent chunk's range.
    auto it_curr = stream_manager_->Find(read_stream);
    bool active = (it_curr != stream_manager_->End()) &&
                  (it_curr->active_ranges.count(id) != 0);
    if (active) {
      evict = ApplyPacingAndCheckEviction(id, chunk_size, it_curr);
    }
    lk.unlock();
    if (active) {
      if (evict) {
        // Complete the evicted range with an error.
        range->OnFinish(Status(StatusCode::kResourceExhausted,
                               "Evicted pre-warmed range due to pacing limit"));
      } else {
        // Deliver data to the range.
        // TODO(#15104) - Consider returning if the range is done, and then
        // skipping CleanupDoneRanges().
        range->OnRead(std::move(range_data), is_transcoded, object_size);
      }
    }
  }
  lk.lock();
  auto it_final = stream_manager_->Find(read_stream);
  if (it_final == stream_manager_->End() || !it_final->stream) return;
  stream_manager_->CleanupDoneRanges(it_final);
  DoRead(std::move(lk), read_stream);
}

bool ObjectDescriptorImpl::ApplyPacingAndCheckEviction(std::int64_t id,
                                                       std::size_t chunk_size,
                                                       StreamIterator it) {
  auto unclaimed_it = unclaimed_ranges_.find(id);
  if (unclaimed_it == unclaimed_ranges_.end()) return false;

  if (total_prewarmed_bytes_buffered_ + chunk_size >
      max_prewarmed_buffer_size_) {
    // Evict the range if it exceeds the pacing limit.
    total_prewarmed_bytes_buffered_ -= unclaimed_it->second.bytes_buffered;
    // Cap tombstone set size to prevent unbounded memory growth in long-lived
    // descriptors where pre-warmed ranges are evicted but never requested.
    if (evicted_ranges_.size() < 1000) {
      evicted_ranges_.insert(unclaimed_it->second.cache_it->first);
    }
    prewarmed_ranges_.erase(unclaimed_it->second.cache_it);
    unclaimed_ranges_.erase(unclaimed_it);

    // Erasing from active_ranges ensures we ignore any subsequent GCS chunks
    // for this range.
    it->active_ranges.erase(id);
    return true;
  }

  // Track buffered data size for pacing.
  unclaimed_it->second.bytes_buffered += chunk_size;
  total_prewarmed_bytes_buffered_ += chunk_size;
  return false;
}

void ObjectDescriptorImpl::DoFinish(
    std::unique_lock<std::mutex> lk,
    std::shared_ptr<ReadStream> const& read_stream,
    std::shared_ptr<OpenStream> const& stream) {
  // Discard finish requests if the stream was already replaced or removed.
  auto it = stream_manager_->Find(read_stream);
  if (it == stream_manager_->End() || !it->stream ||
      it->stream->stream != stream) {
    return;
  }
  it->stream->read_pending = false;
  // Assign CurrentStream to a temporary variable to prevent
  // lifetime extension which can cause the lock to be held until the
  // end of the block.
  std::shared_ptr<OpenStream> current_stream = it->stream->stream;
  lk.unlock();
  future<Status> pending = current_stream->Finish();
  if (!pending.valid()) return;
  pending.then([w = WeakFromThis(), read_stream, current_stream](auto f) {
    if (auto self = w.lock()) {
      self->OnFinish(read_stream, current_stream, f.get());
    }
  });
}

void ObjectDescriptorImpl::OnFinish(
    std::shared_ptr<ReadStream> const& read_stream,
    std::shared_ptr<OpenStream> const& stream, Status const& status) {
  {
    std::unique_lock<std::mutex> lk(mu_);
    // Discard callbacks if cancelled or from stale/removed streams.
    if (cancelled_) return;
    auto it = stream_manager_->Find(read_stream);
    if (it == stream_manager_->End() || !it->stream ||
        it->stream->stream != stream) {
      return;
    }
  }
  google::rpc::Status proto_status = ExtractGrpcStatus(status);

  if (IsResumable(read_stream, status, proto_status)) {
    return Resume(read_stream, proto_status);
  }
  std::unique_lock<std::mutex> lk(mu_);
  // Re-verify stream identity under lock because IsResumable() releases and
  // re-acquires the mutex while notifying range callbacks, during which time
  // another thread or callback could have modified or replaced the stream.
  if (cancelled_) return;
  auto it = stream_manager_->Find(read_stream);
  if (it == stream_manager_->End() || !it->stream ||
      it->stream->stream != stream) {
    return;
  }
  stream_manager_->RemoveStreamAndNotifyRanges(it, status);
  // Since a stream died, we might want to ensure a replacement is queued.
  AssurePendingStreamQueued(lk);
}

void ObjectDescriptorImpl::Resume(
    std::shared_ptr<ReadStream> const& read_stream,
    google::rpc::Status const& proto_status) {
  std::unique_lock<std::mutex> lk(mu_);
  if (cancelled_) return;
  auto it = stream_manager_->Find(read_stream);
  if (it == stream_manager_->End() || !it->stream) return;
  // Set resuming flag to true to prevent any concurrent Flush() from writing
  // to the dying stream while we establish a new one.
  it->stream->resuming = true;
  it->stream->next_request.Clear();
  std::shared_ptr<OpenStream> current_stream = it->stream->stream;
  // This call needs to happen inside the lock, as it may modify
  // `read_object_spec_`.
  ApplyRedirectErrors(read_object_spec_, proto_status);
  google::storage::v2::BidiReadObjectRequest request;
  *request.mutable_read_object_spec() = read_object_spec_;
  for (auto const& kv : it->active_ranges) {
    std::optional<google::storage::v2::ReadRange> range =
        kv.second->RangeForResume(kv.first);
    if (!range) continue;
    *request.add_read_ranges() = *std::move(range);
  }
  lk.unlock();
  make_stream_(std::move(request))
      .then([w = WeakFromThis(), read_stream, current_stream](auto f) {
        if (auto self = w.lock()) {
          self->OnResume(read_stream, current_stream, f.get());
        }
      });
}

void ObjectDescriptorImpl::OnResume(
    std::shared_ptr<ReadStream> const& old_read_stream,
    std::shared_ptr<OpenStream> const& old_stream,
    StatusOr<OpenStreamResult> result) {
  {
    std::unique_lock<std::mutex> lk(mu_);
    if (cancelled_) {
      if (result && result->stream) result->stream->Cancel();
      return;
    }
  }
  if (!result) {
    return OnFinish(old_read_stream, old_stream, std::move(result).status());
  }
  std::unique_lock<std::mutex> lk(mu_);
  // Discard resume responses if cancelled or if the stream entry was removed or
  // already replaced.
  auto it = stream_manager_->Find(old_read_stream);
  if (cancelled_ || it == stream_manager_->End() || !it->stream ||
      it->stream->stream != old_stream) {
    if (result->stream) result->stream->Cancel();
    return;
  }

  // Preserve any Read() range requests that arrived concurrently while the
  // reconnection was in flight.
  google::storage::v2::BidiReadObjectRequest queued_request =
      std::move(it->stream->next_request);

  // Replace the old stream with the new stream and reset policy/state.
  std::shared_ptr<ReadStream> new_read_stream = std::make_shared<ReadStream>(
      std::move(result->stream), resume_policy_prototype_->clone());
  new_read_stream->resume_policy->OnStartSuccess();
  new_read_stream->write_pending = false;
  new_read_stream->read_pending = false;
  new_read_stream->resuming = false;
  new_read_stream->next_request = std::move(queued_request);

  it->stream = new_read_stream;
  std::shared_ptr<OpenStream> new_stream = new_read_stream->stream;

  // TODO(#15105) - this should be done without release the lock.
  // Flush any queued range requests onto the newly active stream.
  Flush(std::move(lk), new_read_stream);
  // Process the first response received during stream establishment.
  OnRead(new_read_stream, new_stream, std::move(result->first_response));
}

bool ObjectDescriptorImpl::IsResumable(
    std::shared_ptr<ReadStream> const& read_stream, Status const& status,
    google::rpc::Status const& proto_status) {
  std::unique_lock<std::mutex> lk(mu_);
  if (cancelled_) return false;
  auto it = stream_manager_->Find(read_stream);
  if (it == stream_manager_->End() || !it->stream) return false;
  for (google::protobuf::Any const& any : proto_status.details()) {
    google::storage::v2::BidiReadObjectError error;
    if (!any.UnpackTo(&error)) continue;

    std::vector<std::pair<std::int64_t, Status>> notify;
    for (auto const& re : error.read_range_errors()) {
      if (it->active_ranges.count(re.read_id()) != 0) {
        notify.emplace_back(re.read_id(), MakeStatusFromRpcError(re.status()));
      }
    }
    if (notify.empty()) continue;

    auto copy = it->active_ranges;
    lk.unlock();
    for (auto const& p : notify) {
      auto l = copy.find(p.first);
      if (l != copy.end()) l->second->OnFinish(p.second);
    }
    lk.lock();
    if (cancelled_) return true;
    auto it_curr = stream_manager_->Find(read_stream);
    if (it_curr == stream_manager_->End() || !it_curr->stream) return true;
    stream_manager_->CleanupDoneRanges(it_curr);
    return true;
  }
  Status effective_status = status;
  if (status.code() == StatusCode::kCancelled) {
    effective_status = Status(StatusCode::kUnavailable, status.message());
  }
  return it->stream->resume_policy->OnFinish(effective_status) ==
         storage::ResumePolicy::kContinue;
}

std::size_t ObjectDescriptorImpl::StreamSize() const {
  return stream_manager_->Size();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
