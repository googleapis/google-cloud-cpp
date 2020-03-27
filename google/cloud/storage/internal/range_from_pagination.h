// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RANGE_FROM_PAGINATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RANGE_FROM_PAGINATION_H

#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

template <typename T, typename Range>
class PaginationIterator {
 public:
  //@{
  /// @name Iterator traits
  using iterator_category = std::input_iterator_tag;
  using value_type = StatusOr<T>;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;
  //@}

  PaginationIterator() : owner_(nullptr) {}

  PaginationIterator& operator++() {
    *this = owner_->GetNext();
    return *this;
  }

  PaginationIterator const operator++(int) {
    PaginationIterator tmp(*this);
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

 private:
  friend Range;

  friend bool operator==(PaginationIterator const& lhs,
                         PaginationIterator const& rhs) {
    // Iterators on different streams are always different.
    if (lhs.owner_ != rhs.owner_) {
      return false;
    }
    // All end iterators are equal.
    if (lhs.owner_ == nullptr) {
      return true;
    }
    // Iterators on the same stream are equal if they point to the same object.
    if (lhs.value_.ok() && rhs.value_.ok()) {
      return *lhs.value_ == *rhs.value_;
    }
    // If one is an error and the other is not then they must be different,
    // because only one iterator per range can have an error status. For the
    // same reason, if both have an error they both are pointing to the same
    // element.
    return lhs.value_.ok() == rhs.value_.ok();
  }

  friend bool operator!=(PaginationIterator const& lhs,
                         PaginationIterator const& rhs) {
    return std::rel_ops::operator!=(lhs, rhs);
  }

  explicit PaginationIterator(Range* owner, value_type value)
      : owner_(owner), value_(std::move(value)) {}

  Range* owner_;
  value_type value_;
};

template <typename T, typename Request, typename Response>
class PaginationRange {
 public:
  explicit PaginationRange(
      Request request,
      std::function<StatusOr<Response>(Request const& r)> loader)
      : request_(std::move(request)),
        next_page_loader_(std::move(loader)),
        next_page_token_(),
        on_last_page_(false) {
    current_ = current_page_.begin();
  }

  /// The iterator type for this Range.
  using iterator = PaginationIterator<T, PaginationRange>;

  /**
   * Return an iterator over the list of hmacKeys.
   *
   * The returned iterator is a single-pass input iterator that reads object
   * metadata from the ListHmacKeysReader when incremented.
   *
   * Creating, and particularly incrementing, multiple iterators on the same
   * ListHmacKeysReader is unsupported and can produce incorrect results.
   */
  iterator begin() { return GetNext(); }

  /// Return an iterator pointing to the end of the stream.
  iterator end() { return PaginationIterator<T, PaginationRange>{}; }

 protected:
  friend class PaginationIterator<T, PaginationRange>;

  /**
   * Fetches (or returns if already fetched) the next object from the stream.
   *
   * @return An iterator pointing to the next element in the stream. On error,
   *   it returns an iterator that is different from `.end()`, but has an error
   *   status. If the stream is exhausted, it returns the `.end()` iterator.
   */
  iterator GetNext() {
    static Status const past_the_end_error(
        StatusCode::kFailedPrecondition,
        "Cannot iterating past the end of ListObjectReader");
    if (current_page_.end() == current_) {
      if (on_last_page_) {
        return iterator(nullptr, past_the_end_error);
      }
      request_.set_page_token(std::move(next_page_token_));
      auto response = next_page_loader_(request_);
      if (!response.ok()) {
        next_page_token_.clear();
        current_page_.clear();
        on_last_page_ = true;
        current_ = current_page_.begin();
        return iterator(this, std::move(response).status());
      }
      next_page_token_ = std::move(response->next_page_token);
      current_page_ = std::move(response->items);
      current_ = current_page_.begin();
      if (next_page_token_.empty()) {
        on_last_page_ = true;
      }
      if (current_page_.end() == current_) {
        return iterator(nullptr, past_the_end_error);
      }
    }
    return iterator(this, std::move(*current_++));
  }

 private:
  Request request_;
  std::function<StatusOr<Response>(Request const& r)> next_page_loader_;
  std::vector<T> current_page_;
  typename std::vector<T>::iterator current_;
  std::string next_page_token_;
  bool on_last_page_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RANGE_FROM_PAGINATION_H
