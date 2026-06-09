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

#include "google/cloud/storage/internal/bucket_metadata_cache.h"
#include <mutex>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void BucketMetadataCache::MoveToFront(std::list<std::string>::iterator it) {
  list_.splice(list_.begin(), list_, it);
}

absl::optional<BucketCacheEntry> BucketMetadataCache::Get(
    std::string const& bucket_name) {
  std::unique_lock<std::mutex> lk(mu_);
  auto it = map_.find(bucket_name);
  if (it == map_.end()) return absl::nullopt;

  MoveToFront(it->second.second);
  return it->second.first;
}

void BucketMetadataCache::Put(std::string const& bucket_name,
                              BucketCacheEntry entry) {
  std::unique_lock<std::mutex> lk(mu_);
  auto it = map_.find(bucket_name);
  if (it != map_.end()) {
    it->second.first = std::move(entry);
    MoveToFront(it->second.second);
    return;
  }

  if (map_.size() >= max_size_) {
    auto oldest = list_.back();
    list_.pop_back();
    map_.erase(oldest);
  }

  list_.push_front(bucket_name);
  map_[bucket_name] = {std::move(entry), list_.begin()};
}

void BucketMetadataCache::Invalidate(std::string const& bucket_name) {
  std::unique_lock<std::mutex> lk(mu_);
  auto it = map_.find(bucket_name);
  if (it != map_.end()) {
    list_.erase(it->second.second);
    map_.erase(it);
  }
}

void BucketMetadataCache::Clear() {
  std::unique_lock<std::mutex> lk(mu_);
  map_.clear();
  list_.clear();
  in_flight_fetch_.clear();
}

bool BucketMetadataCache::StartFetch(std::string const& bucket_name) {
  std::unique_lock<std::mutex> lk(mu_);
  if (in_flight_fetch_.find(bucket_name) != in_flight_fetch_.end()) {
    return false;
  }
  in_flight_fetch_.insert(bucket_name);
  return true;
}

void BucketMetadataCache::EndFetch(std::string const& bucket_name) {
  std::unique_lock<std::mutex> lk(mu_);
  in_flight_fetch_.erase(bucket_name);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
