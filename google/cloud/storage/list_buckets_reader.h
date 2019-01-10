// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_BUCKETS_READER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_BUCKETS_READER_H_

#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/internal/raw_client.h"
#include <iterator>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
class ListBucketsReader;

/**
 * Implements a C++ iterator for listing buckets.
 */
class ListBucketsIterator {
 public:
  //@{
  /// @name Iterator traits
  using iterator_category = std::input_iterator_tag;
  using value_type = BucketMetadata;
  using difference_type = std::ptrdiff_t;
  using pointer = BucketMetadata*;
  using reference = BucketMetadata&;
  //@}

  ListBucketsIterator() : owner_(nullptr) {}

  ListBucketsIterator& operator++();
  ListBucketsIterator const operator++(int) {
    ListBucketsIterator tmp(*this);
    operator++();
    return tmp;
  }

  BucketMetadata const* operator->() const { return value_.operator->(); }
  BucketMetadata* operator->() { return value_.operator->(); }

  BucketMetadata const& operator*() const& { return *value_; }
  BucketMetadata& operator*() & { return *value_; }
#if GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  BucketMetadata const&& operator*() const&& { return *std::move(value_); }
#endif  // GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  BucketMetadata&& operator*() && { return *std::move(value_); }

  bool operator==(ListBucketsIterator const& rhs) const {
    // All end iterators are equal.
    if (owner_ == nullptr) {
      return rhs.owner_ == nullptr;
    }
    // All non-end iterators in the same reader are equal.
    return owner_ == rhs.owner_;
  }

  bool operator!=(ListBucketsIterator const& rhs) const {
    return !(*this == rhs);
  }

 private:
  friend class ListBucketsReader;
  explicit ListBucketsIterator(ListBucketsReader* owner,
                               google::cloud::optional<BucketMetadata> value);

 private:
  ListBucketsReader* owner_;
  google::cloud::optional<BucketMetadata> value_;
};

/**
 * Represents the result of listing a set of Buckets.
 */
class ListBucketsReader {
 public:
  template <typename... Parameters>
  ListBucketsReader(std::shared_ptr<internal::RawClient> client,
                    std::string project_id, Parameters&&... parameters)
      : client_(std::move(client)),
        request_(std::move(project_id)),
        next_page_token_(),
        on_last_page_(false) {
    request_.set_multiple_options(std::forward<Parameters>(parameters)...);
    current_ = current_buckets_.begin();
  }

  /// The iterator type for this stream.
  using iterator = ListBucketsIterator;

  /**
   * Returns an iterator over the list of buckets.
   *
   * The returned iterator is a single-pass input iterator that reads bucket
   * metadata from the BucketListReader when incremented.
   *
   * Creating, and particularly incrementing, multiple iterators on
   * the same RowReader is unsupported and can produce incorrect
   * results.
   *
   * Retry and backoff policies are honored.
   *
   * @throws std::runtime_error if the read failed after retries.
   */
  iterator begin();

  /// Returns an iterator pointing to the end of the stream.
  iterator end() { return ListBucketsIterator(); }

 private:
  friend class ListBucketsIterator;
  /**
   * Fetches (or returns if already fetched) the next bucket from the stream.
   *
   * @return an unset optional if there are no more buckets in the stream.
   */
  google::cloud::optional<BucketMetadata> GetNext();

 private:
  std::shared_ptr<internal::RawClient> client_;
  internal::ListBucketsRequest request_;
  std::vector<BucketMetadata> current_buckets_;
  std::vector<BucketMetadata>::iterator current_;
  std::string next_page_token_;
  bool on_last_page_;
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_BUCKETS_READER_H_
