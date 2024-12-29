// Copyright 2024 Google LLC
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

#include "google/cloud/bigtable/emulator/filter.h"
#include "google/cloud/status_or.h"
#include "google/cloud/internal/make_status.h"
#include <re2/re2.h>
#include <random>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

CellView const& CellStream::operator++() {
  Next();
  return Value();
}

CellView CellStream::operator++(int) {
  CellView tmp = Value();
  Next();
  return tmp;
}

template <typename State, typename FilterFunctor, typename StateResetFunctor>
class PerRowStatusFilter {
 public:
  PerRowStatusFilter(CellStream source, FilterFunctor filter,
                     StateResetFunctor reset)
      : filter_(std::move(filter)),
        reset_(std::move(reset)),
        source_(std::move(source)) {}

  absl::optional<CellView> operator()() {
    for (; source_; ++source_) {
      if (!prev_row_ ||
           !(&prev_row_.value().get() == &source_->row_key() ||
           prev_row_.value().get() == source_->row_key())) {
        state_ = reset_();
        prev_row_ = source_->row_key();
      }
      if (filter_(state_, source_.Value())) {
        return source_++;
      }
    }
    return {};
  }
 private:
  absl::optional<std::reference_wrapper<std::string const>> prev_row_;
  State state_;
  FilterFunctor filter_; 
  StateResetFunctor reset_;
  CellStream source_;
};


template <typename Filter>
CellStream MakeTrivialFilter(CellStream source, Filter filter) {
  return CellStream(
      [source = std::move(source),
       filter = std::move(filter)]() mutable -> absl::optional<CellView> {
        for (; source && !filter(*source); ++source);
        if (!source) {
          return {};
        }
        return source++;
      });
}

StatusOr<CellStream> CreateFilter(
    ::google::bigtable::v2::RowFilter const& filter, CellStream source) {
  if (filter.has_pass_all_filter()) {
    if (!filter.pass_all_filter()) {
      return InvalidArgumentError(
          "`pass_all_filter` explicitly set to `false`.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    return source;
  }
  if (filter.has_block_all_filter()) {
    if (!filter.block_all_filter()) {
      return InvalidArgumentError(
          "`block_all_filter` explicitly set to `false`.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    return CellStream([]() -> absl::optional<CellView> { return {}; });
  }
  if (filter.has_row_key_regex_filter()) {
    std::cout << "Regex filter: " << filter.row_key_regex_filter() << std::endl;
    auto pattern = std::make_shared<re2::RE2>(filter.row_key_regex_filter());
    if (!pattern->ok()) {
      return InvalidArgumentError(
          "`row_key_regex_filter` is not a valid RE2 regex.",
          GCP_ERROR_INFO()
              .WithMetadata("filter", filter.DebugString())
              .WithMetadata("description", pattern->error()));
    }
    return MakeTrivialFilter(
        std::move(source),
        [pattern = std::move(pattern)](CellView const& cell_view) mutable {
          return re2::RE2::PartialMatch(cell_view.row_key(), *pattern);
        });
  }
  if (filter.has_row_sample_filter()) {
    double pass_prob = filter.row_sample_filter();
    if (pass_prob + std::numeric_limits<double>::epsilon() < 0
      || pass_prob - std::numeric_limits<double>::epsilon() > 1) {
      return InvalidArgumentError(
          "`row_sample_filter` is not a valid probability.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    auto per_row_filter = [] (bool skip, CellView const &) {
      return !skip;
    };
    std::mt19937 gen;
    auto reset_state = [gen = std::move(gen), pass_prob]() mutable {
      std::uniform_real_distribution<double> dis(0.0, 1.0);
      return dis(gen) > pass_prob;
    };
    return CellStream(PerRowStatusFilter<bool, decltype(per_row_filter),
                                         decltype(reset_state)>(
        std::move(source), std::move(per_row_filter), std::move(reset_state)));
  }
  if (filter.has_family_name_regex_filter()) {
    auto pattern = std::make_shared<re2::RE2>(filter.family_name_regex_filter());
    if (!pattern->ok()) {
      return InvalidArgumentError(
          "`family_name_regex_filter` is not a valid RE2 regex.",
          GCP_ERROR_INFO()
              .WithMetadata("filter", filter.DebugString())
              .WithMetadata("description", pattern->error()));
    }
    return CellStream(
        [pattern = std::move(pattern),
         source = std::move(source)]() mutable -> absl::optional<CellView> {
          for (; source &&
                 !re2::RE2::PartialMatch(source->column_family(), *pattern);
               ++source) {
          }
          if (!source) {
            return {};
          }
          return source++;
        });
  }
  if (filter.has_column_qualifier_regex_filter()) {
    auto pattern =
        std::make_shared<re2::RE2>(filter.column_qualifier_regex_filter());
    if (!pattern->ok()) {
      return InvalidArgumentError(
          "`column_qualifier_regex_filter` is not a valid RE2 regex.",
          GCP_ERROR_INFO()
              .WithMetadata("filter", filter.DebugString())
              .WithMetadata("description", pattern->error()));
    }
    return CellStream(
        [pattern = std::move(pattern),
         source = std::move(source)]() mutable -> absl::optional<CellView> {
          for (; source &&
                 !re2::RE2::PartialMatch(source->column_qualifier(), *pattern);
               ++source) {
          }
          if (!source) {
            return {};
          }
          return source++;
        });
  }
  if (filter.has_cells_per_row_offset_filter()) {
    std::int64_t cells_per_row_offset = filter.cells_per_row_offset_filter();
    if (cells_per_row_offset < 0) {
      return InvalidArgumentError(
          "`cells_per_row_offset_filter` is negative.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    auto per_row_filter = [] (std::int64_t &skip, CellView const &) {
      return skip-- <= 0;
    };
    auto reset_state = [cells_per_row_offset]() {
      return cells_per_row_offset;
    };
    return CellStream(PerRowStatusFilter<std::int64_t, decltype(per_row_filter),
                                         decltype(reset_state)>(
        std::move(source), std::move(per_row_filter), std::move(reset_state)));
  }
  //  ColumnRange column_range_filter = 7;
  //  TimestampRange timestamp_range_filter = 8;
  //  bytes value_regex_filter = 9;
  //  ValueRange value_range_filter = 15;
  //  int32 cells_per_row_offset_filter = 10;
  //  int32 cells_per_row_limit_filter = 11;
  //  int32 cells_per_column_limit_filter = 12;
  //  bool strip_value_transformer = 13;
  //  string apply_label_transformer = 19;
  if (filter.has_chain()) {
    CellStream res = std::move(source);
    for (auto const &subfilter : filter.chain().filters()) {
      auto maybe_res = CreateFilter(subfilter, std::move(res));
      if (!maybe_res) {
        return maybe_res.status();
      }
      res = *std::move(maybe_res);
    }
    return res;
  }
  //  Interleave interleave = 2;
  //  Condition condition = 3;
  //  bool sink = 16;
  return UnimplementedError(
      "Unsupported filter.",
      GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));

}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
