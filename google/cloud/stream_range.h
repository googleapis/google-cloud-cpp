// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STREAM_RANGE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STREAM_RANGE_H

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"
#include <functional>
#include <iterator>
#include <utility>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

// Defined below.
template <typename T>
class StreamRange;

namespace internal {

/**
 * A function that repeatedly returns `T`s, and ends with a `Status`.
 *
 * This function should return instances of `T` from its underlying stream
 * until there are no more. The end-of-stream is indicated by returning a
 * `Status` indicating either success or an error. This function will not be
 * invoked any more after it returns any `Status`.
 *
 * @par Example `StreamReader` that returns the integers from 1-10.
 *
 * @code
 * int counter = 0;
 * auto reader = [&counter]() -> StreamReader<int>::result_type {
 *   if (++counter <= 10) return counter;
 *   return Status{};  // OK
 * };
 * @endcode
 */
template <typename T>
using StreamReader = std::function<absl::variant<Status, T>()>;

// Defined below.
template <typename T>
StreamRange<T> MakeStreamRange(StreamReader<T>);

}  // namespace internal

/**
 * A `StreamRange<T>` puts a range-like interface on a stream of `T` objects.
 *
 * Callers can iterate the range using its `begin()` and `end()` members to
 * access iterators that will work with any normal C++ constructs and
 * algorithms that accept [Input Iterators][input-iter-link].
 *
 * Callers should only consume/iterate this range. There is no public way for a
 * caller to construct a non-empty instance.
 *
 * @par Example: Iterating a range of 10 integers
 *
 * @code
 * // Some function that returns a StreamRange<int>
 * StreamRange<int> MakeRangeFromOneTo(int n);
 *
 * StreamRange<int> sr = MakeRangeFromOneTo(10);
 * for (int x : sr) {
 *   std::cout << x << "\n";
 * }
 * @endcode
 *
 * [input-iter-link]: https://en.cppreference.com/w/cpp/named_req/InputIterator
 */
template <typename T>
class StreamRange {
 public:
  /**
   * An input iterator for a `StreamRange<T>` -- DO NOT USE DIRECTLY.
   *
   * Use `StreamRange::iterator` instead.
   */
  template <typename U>
  class IteratorImpl {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = U;
    using difference_type = std::size_t;
    using reference = value_type&;
    using pointer = value_type*;
    using const_reference = value_type const&;
    using const_pointer = value_type const*;

    /// Constructs an "end" iterator.
    explicit IteratorImpl() = default;

    reference operator*() { return owner_->current_; }
    pointer operator->() { return &owner_->current_; }
    const_reference operator*() const { return owner_->current_; }
    const_pointer operator->() const { return &owner_->current_; }

    IteratorImpl& operator++() {
      owner_->Next();
      is_end_ = owner_->is_end_;
      return *this;
    }

    IteratorImpl operator++(int) {
      auto copy = *this;
      ++*this;
      return copy;
    }

    friend bool operator==(IteratorImpl const& a, IteratorImpl const& b) {
      return a.is_end_ == b.is_end_;
    }

    friend bool operator!=(IteratorImpl const& a, IteratorImpl const& b) {
      return !(a == b);
    }

   private:
    friend class StreamRange;
    explicit IteratorImpl(StreamRange* owner)
        : owner_(owner), is_end_(owner_->is_end_) {}
    StreamRange* owner_;
    bool is_end_ = true;
  };

  using value_type = StatusOr<T>;
  using iterator = IteratorImpl<value_type>;
  using difference_type = typename iterator::difference_type;
  using reference = typename iterator::reference;
  using pointer = typename iterator::pointer;
  using const_reference = typename iterator::const_reference;
  using const_pointer = typename iterator::const_pointer;

  /**
   * Default-constructs an empty range.
   */
  StreamRange() = default;

  //@{
  // @name Move-only
  StreamRange(StreamRange const&) = delete;
  StreamRange& operator=(StreamRange const&) = delete;
  // NOLINTNEXTLINE(performance-noexcept-move-constructor)
  StreamRange(StreamRange&&) = default;
  // NOLINTNEXTLINE(performance-noexcept-move-constructor)
  StreamRange& operator=(StreamRange&&) = default;
  //@}

  iterator begin() { return iterator(this); }
  iterator end() { return iterator(); }

 private:
  void Next() {
    // Jump to the end if we previously got an error.
    if (!is_end_ && !current_) {
      is_end_ = true;
      return;
    }
    struct UnpackVariant {
      StreamRange& sr;
      void operator()(Status&& status) {
        sr.is_end_ = status.ok();
        if (!status.ok()) sr.current_ = std::move(status);
      }
      void operator()(T&& t) {
        sr.is_end_ = false;
        sr.current_ = std::move(t);
      }
    };
    auto v = reader_();
    absl::visit(UnpackVariant{*this}, std::move(v));
  }

  template <typename U>
  friend StreamRange<U> internal::MakeStreamRange(internal::StreamReader<U>);

  /**
   * Constructs a `StreamRange<T>` that will use the given @p reader.
   *
   * The `T` objects are read from the caller-provided `internal::StreamReader`
   * functor, which is invoked repeatedly as the range is iterated. The
   * `internal::StreamReader` can return an OK `Status` to indicate a successful
   * end of stream, or a non-OK `Status` to indicate an error, or a `T`. The
   * `internal::StreamReader` will not be invoked again after it returns a
   * `Status`.
   *
   * @par Example: Printing integers from 1-10.
   *
   * @code
   * int counter = 0;
   * auto reader = [&counter]() -> internal::StreamReader<int>::result_type {
   *   if (++counter <= 10) return counter;
   *   return Status{};
   * };
   * StreamRange<int> sr(std::move(reader));
   * for (int x : sr) {
   *   std::cout << x << "\n";
   * }
   * @endcode
   *
   * @param reader must not be nullptr.
   */
  explicit StreamRange(internal::StreamReader<T> reader)
      : reader_(std::move(reader)) {
    Next();
  }

  internal::StreamReader<T> reader_;
  StatusOr<T> current_;
  bool is_end_ = true;
};

namespace internal {

/**
 * Factory to construct a `StreamRange<T>` with the given `StreamReader<T>`.
 *
 * Callers should explicitly specify the `T` parameter when calling this
 * function template so that lambdas will implicitly convert to the underlying
 * `StreamReader<T>` (i.e., std::function). For example:
 *
 * @code
 * StreamReader<int> empty = MakeStreamReader<int>([] { return Status{}; });
 * @endcode
 */
template <typename T>
StreamRange<T> MakeStreamRange(StreamReader<T> reader) {
  return StreamRange<T>{std::move(reader)};
}

}  // namespace internal

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STREAM_RANGE_H
