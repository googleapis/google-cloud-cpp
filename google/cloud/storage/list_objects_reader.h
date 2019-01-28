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

#include "google/cloud/status_or.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/raw_client.h"
#include <iterator>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
class ListObjectsReader;

/**
 * Implements a C++ iterator for listing objects.
 */
class ListObjectsIterator {
 public:
  //@{
  /// @name Iterator traits
  using iterator_category = std::input_iterator_tag;
  using value_type = StatusOr<ObjectMetadata>;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;
  //@}

  ListObjectsIterator() : owner_(nullptr) {}

  ListObjectsIterator& operator++();
  ListObjectsIterator const operator++(int) {
    ListObjectsIterator tmp(*this);
    operator++();
    return tmp;
  }

  value_type const* operator->() const { return &value_; }
  value_type* operator->() { return &value_; }

  value_type const& operator*() const& { return value_; }
  value_type& operator*() & { return value_; }
#if GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  value_type const&& operator*() const&& { return std::move(value_); }
#endif  // GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  value_type&& operator*() && { return std::move(value_); }

  friend bool operator==(ListObjectsIterator const& lhs,
                         ListObjectsIterator const& rhs) {
    // All end iterators are equal.
    if (lhs.owner_ == nullptr) {
      return rhs.owner_ == nullptr;
    }
    // Iterators on different streams are always different.
    if (lhs.owner_ != rhs.owner_) {
      return false;
    }
    // Iterators on the same stream are equal if they point to the same object.
    if (lhs.value_.ok() && rhs.value_.ok()) {
      return lhs.value_.value() == rhs.value_.value();
    }
    return lhs.value_.status() == rhs.value_.status();
  }

  friend bool operator!=(ListObjectsIterator const& lhs,
                         ListObjectsIterator const& rhs) {
    return !(lhs == rhs);
  }

 private:
  friend class ListObjectsReader;
  explicit ListObjectsIterator(ListObjectsReader* owner, value_type value);

 private:
  ListObjectsReader* owner_;
  value_type value_;
};

/**
 * Represents the result of listing a set of Objects.
 */
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
   * metadata from the ListObjectsReader when incremented.
   *
   * Creating, and particularly incrementing, multiple iterators on the same
   * ListObjectsReader is unsupported and can produce incorrect results.
   */
  iterator begin();

  /// Return an iterator pointing to the end of the stream.
  iterator end() { return ListObjectsIterator(); }

 private:
  friend class ListObjectsIterator;
  /**
   * Fetches (or returns if already fetched) the next object from the stream.
   *
   * @return An iterator pointing to the next element in the stream. On error,
   *   it returns an iterator that is different from `.end()`, but has an error
   *   status. If the stream is exhausted, it returns the `.end()` iterator.
   */
  ListObjectsIterator GetNext();

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
