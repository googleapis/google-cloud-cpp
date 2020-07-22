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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_ROW_SET_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_ROW_SET_H

#include "google/cloud/bigquery/row.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
template <typename RowType>
class RowSet {
 public:
  template <typename T>
  class Iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    reference operator*() { return curr_; }
    pointer operator->() { return &curr_; }

    Iterator& operator++() {
      Advance();
      return *this;
    }

    Iterator operator++(int) {
      auto const old = *this;
      operator++();
      return old;
    }

    friend bool operator==(Iterator const& lhs, Iterator const rhs) {
      bool const lhs_done = !lhs.source_;
      bool const rhs_done = !rhs.source_;
      return lhs_done == rhs_done;
    }

    friend bool operator!=(Iterator const& lhs, Iterator const& rhs) {
      return !(lhs == rhs);
    }

   private:
    friend RowSet;
    explicit Iterator(
        std::function<StatusOr<absl::optional<RowType>>()>* source)
        : source_(source) {
      if (source_) {
        Advance();
      }
    }

    void Advance() {
      auto next = std::move((*source_)());
      if (!next.ok()) {
        curr_ = next.status();
        source_ = nullptr;
      } else if (!next.value()) {
        source_ = nullptr;
      } else {
        curr_ = *next.value();
      }
    }

    std::function<StatusOr<absl::optional<RowType>>()>* source_;
    StatusOr<RowType> curr_;
  };

  using value_type = StatusOr<RowType>;
  using iterator = Iterator<value_type>;

  explicit RowSet(std::function<StatusOr<absl::optional<RowType>>()> source)
      : source_(std::move(source)) {}

  iterator begin() { return iterator(&source_); }

  iterator end() { return iterator(nullptr); }

 private:
  std::function<StatusOr<absl::optional<RowType>>()> source_;
};

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_ROW_SET_H
