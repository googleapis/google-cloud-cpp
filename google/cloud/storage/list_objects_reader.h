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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_OBJECTS_READER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_OBJECTS_READER_H_

#include "google/cloud/storage/internal/list_objects_request.h"
#include "google/cloud/storage/internal/raw_client.h"
#include <iterator>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
class ListObjectsReader;

/**
 * A class meeting C++'s InputIterator requirements for listing objects.
 */
class ListObjectsIterator
    : public std::iterator<std::input_iterator_tag, ObjectMetadata> {
 public:
  ListObjectsIterator() : owner_(nullptr) {}

  ListObjectsIterator& operator++();
  ListObjectsIterator const operator++(int) {
    ListObjectsIterator tmp(*this);
    operator++();
    return tmp;
  }

  ObjectMetadata const* operator->() const { return value_.operator->(); }
  ObjectMetadata* operator->() { return value_.operator->(); }

  ObjectMetadata const& operator*() const& { return *value_; }
  ObjectMetadata& operator*() & { return *value_; }
  ObjectMetadata const&& operator*() const&& { return *std::move(value_); }
  ObjectMetadata&& operator*() && { return *std::move(value_); }

  bool operator==(ListObjectsIterator const& rhs) const {
    // All end iterators are equal.
    if (owner_ == nullptr) {
      return rhs.owner_ == nullptr;
    }
    // All non-end iterators in the same reader are equal.
    return owner_ == rhs.owner_;
  }

  bool operator!=(ListObjectsIterator const& rhs) const {
    return !(*this == rhs);
  }

 private:
  friend class ListObjectsReader;
  explicit ListObjectsIterator(
      ListObjectsReader* owner,
      google::cloud::internal::optional<ObjectMetadata> value);

 private:
  ListObjectsReader* owner_;
  google::cloud::internal::optional<ObjectMetadata> value_;
};

class ListObjectsReader {
 public:
  template <typename... Parameters>
  ListObjectsReader(std::shared_ptr<internal::RawClient> client,
                    std::string bucket_name, Parameters&&... parameters)
      : client_(std::move(client)),
        request_(std::move(bucket_name)),
        next_page_token_(),
        on_last_page_(false) {
    request_.set_multiple_options(std::forward<Parameters>(parameters)...);
    current_ = current_objects_.begin();
  }

  /// The iterator type for this stream.
  using iterator = ListObjectsIterator;

  /**
   * Return an iterator over the list of objects.
   *
   * The returned iterator is a single-pass input iterator that reads object
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

  /// Return an iterator pointing to the end of the stream.
  iterator end() { return ListObjectsIterator(); }

 private:
  friend class ListObjectsIterator;
  /**
   * Fetch (or return if already fetched) the next object from the stream.
   *
   * @return an unset optional if there are no more objects in the stream.
   */
  google::cloud::internal::optional<ObjectMetadata> GetNext();

 private:
  std::shared_ptr<internal::RawClient> client_;
  internal::ListObjectsRequest request_;
  std::vector<ObjectMetadata> current_objects_;
  std::vector<ObjectMetadata>::iterator current_;
  std::string next_page_token_;
  bool on_last_page_;
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_OBJECTS_READER_H_
