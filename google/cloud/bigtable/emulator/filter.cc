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
#include "google/cloud/bigtable/internal/google_bytes_traits.h"
#include <re2/re2.h>
#include <random>
#include <queue>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {
bool StringRefEq(std::string const &s1, std::string const &s2) {
  return &s1 == &s2 || s1 == s2;
}
}  // namespace

FilterContext& FilterContext::DisallowApplyLabel() {
  allow_apply_label_ = false;
  return *this;
}

void CellStream::operator++() {
  Next();
}

CellView CellStream::operator++(int) {
  CellView tmp = Value();
  Next();
  return tmp;
}

template <typename FilterFunctor, typename StateResetFunctor>
class PerRowStateFilter {
  static_assert(std::is_invocable_v<StateResetFunctor()>,
                "StateResetFunctor must be invocable with no arguments");
  using State = std::decay_t<std::result_of_t<StateResetFunctor()>>;
  static_assert(std::is_default_constructible_v<State>,
                "State must be default constructible");
  static_assert(std::is_assignable_v<State&, State>,
                "State must assignable");
  static_assert(
      std::is_same_v<
          std::result_of_t<FilterFunctor(State&, CellView const&)>,
          bool>,
      "The result of FilterFunctor invocation must be a `bool`");

 public:
  PerRowStateFilter(FilterFunctor filter, StateResetFunctor reset)
      : filter_(std::move(filter)), reset_(std::move(reset)) {}

  bool operator()(CellView const &cell_view) {
    if (!prev_row_ ||
        !StringRefEq(prev_row_.value().get(), cell_view.row_key())) {
      state_ = reset_();
      prev_row_ = cell_view.row_key();
    }
    return filter_(state_, cell_view);
  }
 private:
  absl::optional<std::reference_wrapper<std::string const>> prev_row_;
  State state_;
  FilterFunctor filter_; 
  StateResetFunctor reset_;
};

template <typename FilterFunctor, typename StateResetFunctor>
class PerColumnStateFilter {
  static_assert(std::is_invocable_v<StateResetFunctor()>,
                "StateResetFunctor must be invocable with no arguments");
  using State = std::decay_t<std::result_of_t<StateResetFunctor()>>;
  static_assert(std::is_default_constructible_v<State>,
                "State must be default constructible");
  static_assert(std::is_assignable_v<State&, State>,
                "State must assignable");
  static_assert(
      std::is_same_v<
          std::result_of_t<FilterFunctor(State&, CellView const&)>,
          bool>,
      "The result of FilterFunctor invocation must be a `bool`");

 public:
  PerColumnStateFilter(FilterFunctor filter, StateResetFunctor reset)
      : filter_(std::move(filter)), reset_(std::move(reset)) {}

  bool operator()(CellView const &cell_view) {
    if (!prev_|| !prev_->Matches(cell_view)) {
      state_ = reset_();
      prev_ = Prev(cell_view);
    }
    return filter_(state_, cell_view);
  }
 private:
  class Prev {
   public:
    Prev(CellView const& cell_view)
        : row_key_(cell_view.row_key()),
          column_family_(cell_view.column_family()),
          column_qualifier_(cell_view.column_qualifier()) {}

    bool Matches(CellView const &cell_view) {
      return StringRefEq(row_key_.get(), cell_view.row_key()) &&
             StringRefEq(column_family_.get(), cell_view.column_family()) &&
             StringRefEq(column_qualifier_, cell_view.column_qualifier());
    }

   private:
    std::reference_wrapper<std::string const> row_key_;
    std::reference_wrapper<std::string const> column_family_;
    std::reference_wrapper<std::string const> column_qualifier_;
  };
  absl::optional<Prev> prev_;
  State state_;
  FilterFunctor filter_; 
  StateResetFunctor reset_;
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

template <typename FilterFunctor, typename StateResetFunctor>
CellStream MakePerRowStateFilter(CellStream source, FilterFunctor filter,
                                 StateResetFunctor state_reset) {
  return MakeTrivialFilter(std::move(source),
                           PerRowStateFilter<FilterFunctor, StateResetFunctor>(
                               std::move(filter), std::move(state_reset)));
}

template <typename FilterFunctor, typename StateResetFunctor>
CellStream MakePerColumnStateFilter(CellStream source, FilterFunctor filter,
                                    StateResetFunctor state_reset) {
  return MakeTrivialFilter(
      std::move(source), PerColumnStateFilter<FilterFunctor, StateResetFunctor>(
                             std::move(filter), std::move(state_reset)));
}

class ValueRangeFilter {
 public:
  ValueRangeFilter(::google::bigtable::v2::ColumnRange const &column_range) : 
    string_cmp_(internal::CompareColumnQualifiers)
  {
    if (column_range.has_start_qualifier_closed()) {
      start_ = column_range.start_qualifier_closed();
      start_closed_ = true;
    } else if (column_range.has_start_qualifier_open()) {
      start_ = column_range.start_qualifier_open();
      start_closed_ = false;
    } else {
      start_closed_ = true;
    }
    if (column_range.has_end_qualifier_closed()) {
      end_ = column_range.end_qualifier_closed();
      end_closed_ = true;
      has_end_ = true;
    } else if (column_range.has_end_qualifier_open()) {
      end_ = column_range.end_qualifier_open();
      end_closed_ = false;
      has_end_ = true;
    } else {
      has_end_ = false;
    }
  }

  ValueRangeFilter(::google::bigtable::v2::ValueRange const& value_range)
      : string_cmp_(internal::CompareCellValues) {
    if (value_range.has_start_value_closed()) {
      start_ = value_range.start_value_closed();
      start_closed_ = true;
    } else if (value_range.has_start_value_open()) {
      start_ = value_range.start_value_open();
      start_closed_ = false;
    } else {
      start_closed_ = true;
    }
    if (value_range.has_end_value_closed()) {
      end_ = value_range.end_value_closed();
      end_closed_ = true;
      has_end_ = true;
    } else if (value_range.has_end_value_open()) {
      end_ = value_range.end_value_open();
      end_closed_ = false;
      has_end_ = true;
    } else {
      has_end_ = false;
    }
  }

  bool WithinRange(std::string const &val) const {
    if (start_closed_) {
      if (string_cmp_(start_, val) > 0) {
        return false;
      }
    } else {
      if (string_cmp_(start_, val) >= 0) {
        return false;
      }
    }
    if (!has_end_) {
      return true;
    }
    if (end_closed_) {
      if (string_cmp_(val, end_) > 0) {
        return false;
      }
    } else {
      if (string_cmp_(val, end_) >= 0) {
        return false;
      }
    }
    return true;
  }

 private:
  std::function<int(std::string const&, std::string const& rhs)> string_cmp_;
  std::string start_;
  std::string end_;
  bool start_closed_;
  bool has_end_;
  bool end_closed_;
};


class MergeCellStreams {
 public:
  class CellStreamGreater {
   public:
    bool operator()(std::shared_ptr<CellStream> const& lhs,
                    std::shared_ptr<CellStream> const& rhs) const {
      auto row_key_cmp =
          internal::CompareRowKey((*lhs)->row_key(), (*rhs)->row_key());
      if (row_key_cmp != 0) {
        return row_key_cmp > 0;
      }
      auto cf_cmp = internal::CompareColumnQualifiers((*lhs)->column_family(),
                                                      (*rhs)->column_family());
      if (cf_cmp != 0) {
        return cf_cmp > 0;
      }
      auto col_cmp = internal::CompareColumnQualifiers(
          (*lhs)->column_qualifier(), (*rhs)->column_qualifier());
      if (col_cmp != 0) {
        return col_cmp > 0;
      }
      return (*lhs)->timestamp() > (*rhs)->timestamp();
    }
  };

  MergeCellStreams(std::vector<CellStream> streams) {
    for (auto &stream : streams) {
      if (stream.HasValue()) {
        unfinished_streams_.emplace(
            std::make_shared<CellStream>(std::move(stream)));
      }
    }
  }

  absl::optional<CellView> operator()() {
    if (unfinished_streams_.empty()) {
      return {};
    }
    auto stream_to_advance = unfinished_streams_.top();
    unfinished_streams_.pop();
    CellView res = stream_to_advance->Value();
    stream_to_advance->Next();
    if (stream_to_advance->HasValue()) {
      unfinished_streams_.emplace(std::move(stream_to_advance));
    }
    return res;
  }

  std::priority_queue<std::shared_ptr<CellStream>,
                      std::vector<std::shared_ptr<CellStream>>,
                      CellStreamGreater>
      unfinished_streams_;
};

class ConditionStream {
 public:
  ConditionStream(CellStream source, CellStream predicate,
                  CellStream true_stream, CellStream false_stream)
      : source_(std::move(source)),
        predicate_stream_(std::move(predicate)),
        true_stream_(std::move(true_stream)),
        false_stream_(std::move(false_stream)) {}

  absl::optional<CellView> operator()() {
    while (true) {
      auto cell_view = *source_;

      if (!prev_row_ ||
          !StringRefEq(prev_row_.value().get(), cell_view.row_key())) {
        prev_row_ = cell_view.row_key();
        condition_true_.reset();
      }
      if (!condition_true_.has_value()) {
        // Let's test if the predicate stream returned something for this row.
        for (; predicate_stream_ &&
               internal::CompareRowKey(predicate_stream_->row_key(),
                                       cell_view.row_key()) < 0;
             predicate_stream_.Next());
        if (predicate_stream_ &&
            internal::CompareRowKey(predicate_stream_->row_key(),
                                    cell_view.row_key()) == 0) {
          // Predicate stream did return somthing for this row.
          condition_true_ = true;
          // Fast-forward the true stream to start at current row.
          for (;
               true_stream_ && internal::CompareRowKey(true_stream_->row_key(),
                                                       cell_view.row_key()) < 0;
               true_stream_.Next());
        } else {
          // Predicate stream did not return anything for this row.
          condition_true_ = false;
          // Fast-forward the false stream to start at current row.
          for (; false_stream_ &&
                 internal::CompareRowKey(false_stream_->row_key(),
                                         cell_view.row_key()) < 0;
               false_stream_.Next());
        }
      }
      if (*condition_true_) {
        if (true_stream_ && internal::CompareRowKey(true_stream_->row_key(),
                                                    cell_view.row_key()) == 0) {
          return true_stream_++;
        }
      } else {
        if (false_stream_ &&
            internal::CompareRowKey(false_stream_->row_key(),
                                    cell_view.row_key()) == 0) {
          return false_stream_++;
        }
      }
      // True/false stream exhausted, reset state and fast-forward source.
      condition_true_.reset();
      for (;
           source_ && internal::CompareRowKey(source_->row_key(),
                                              prev_row_->get()) == 0;
           source_.Next());
      if (!source_) {
        return {};
      }
    }
  }

 private:
  CellStream source_;
  CellStream predicate_stream_;
  CellStream true_stream_;
  CellStream false_stream_;
  absl::optional<std::reference_wrapper<std::string const>> prev_row_;
  absl::optional<bool> condition_true_;
};

StatusOr<CellStream> CreateFilterImpl(
    ::google::bigtable::v2::RowFilter const& filter, CellStream source,
    FilterContext const& ctx, std::vector<CellStream> &direct_sinks) {
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
  if (filter.has_value_regex_filter()) {
    auto pattern = std::make_shared<re2::RE2>(filter.value_regex_filter());
    if (!pattern->ok()) {
      return InvalidArgumentError(
          "`value_regex_filter` is not a valid RE2 regex.",
          GCP_ERROR_INFO()
              .WithMetadata("filter", filter.DebugString())
              .WithMetadata("description", pattern->error()));
    }
    return MakeTrivialFilter(
        std::move(source),
        [pattern = std::move(pattern)](CellView const& cell_view) mutable {
          return re2::RE2::PartialMatch(cell_view.value(), *pattern);
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
    return MakePerRowStateFilter(
        std::move(source),
        [](bool& should_pass, CellView const&) { return should_pass; },
        [gen = std::mt19937(), pass_prob]() mutable {
          std::uniform_real_distribution<double> dis(0.0, 1.0);
          return dis(gen) < pass_prob;
        });
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
    return MakeTrivialFilter(
        std::move(source),
        [pattern = std::move(pattern)](CellView const& cell_view) mutable {
          return re2::RE2::PartialMatch(cell_view.column_family(), *pattern);
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
    return MakeTrivialFilter(
        std::move(source),
        [pattern = std::move(pattern)](CellView const& cell_view) mutable {
          return re2::RE2::PartialMatch(cell_view.column_qualifier(), *pattern);
        });
  }
  if (filter.has_column_range_filter()) {
    return MakeTrivialFilter(
        std::move(source),
        [qualifier_filter = ValueRangeFilter(filter.column_range_filter()),
         column_family = filter.column_range_filter().family_name()](
            CellView const& cell_view) {
          return cell_view.column_family() == column_family &&
                 qualifier_filter.WithinRange(cell_view.column_qualifier());
        });
  }
  if (filter.has_value_range_filter()) {
    return MakeTrivialFilter(
        std::move(source),
        [value_filter = ValueRangeFilter(filter.value_range_filter())](
            CellView const& cell_view) {
          return value_filter.WithinRange(cell_view.value());
        });
  }
  if (filter.has_cells_per_row_offset_filter()) {
    std::int64_t cells_per_row_offset = filter.cells_per_row_offset_filter();
    if (cells_per_row_offset < 0) {
      return InvalidArgumentError(
          "`cells_per_row_offset_filter` is negative.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    return MakePerRowStateFilter(
        std::move(source),
        [](std::int64_t& per_row_state, CellView const&) {
          return per_row_state-- <= 0;
        },
        [cells_per_row_offset]() { return cells_per_row_offset; });
  }
  if (filter.has_cells_per_row_limit_filter()) {
    std::int64_t cells_per_row_limit = filter.cells_per_row_limit_filter();
    if (cells_per_row_limit < 0) {
      return InvalidArgumentError(
          "`cells_per_row_limit_filter` is negative.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    return MakePerRowStateFilter(
        std::move(source),
        [cells_per_row_limit](std::int64_t& per_row_state, CellView const&) {
          return per_row_state++ < cells_per_row_limit;
        },
        []() -> std::int64_t { return 0; });
  }
  if (filter.has_cells_per_column_limit_filter()) {
    std::int64_t cells_per_column_limit = filter.cells_per_column_limit_filter();
    if (cells_per_column_limit < 0) {
      return InvalidArgumentError(
          "`cells_per_column_limit_filter` is negative.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    return MakePerColumnStateFilter(
        std::move(source),
        [cells_per_column_limit](std::int64_t& per_column_state, CellView const&) {
          return per_column_state++ < cells_per_column_limit;
        },
        []() -> std::int64_t { return 0; });
  }
  if (filter.has_timestamp_range_filter()) {
    auto const & ts_filter = filter.timestamp_range_filter();
    return MakeTrivialFilter(
        std::move(source),
        [start = ts_filter.start_timestamp_micros(),
         end = ts_filter.end_timestamp_micros()](CellView const& cell_view) {
          auto timestamp_micros =
              std::chrono::duration_cast<std::chrono::microseconds>(
                  cell_view.timestamp())
                  .count();

          return timestamp_micros >= start &&
                 (end == 0 || timestamp_micros < end);
        });
  }
  if (filter.has_apply_label_transformer()) {
    if (!ctx.IsApplyLabelAllowed()) {
      return InvalidArgumentError(
          "Two `apply_label_transformer`s cannot coexist in one chain.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    return CellStream([source = std::move(source),
                       label = std::make_shared<std::string>(
                           filter.apply_label_transformer())]() mutable
                      -> absl::optional<CellView> {
      if (!source) {
        return {};
      }
      CellView res = source++;
      std::cout << "Label " << label
                << " being set on cell value: " << res.value() << std::endl;
      res.SetLabel(*label);
      return res;
    });
  }
  if (filter.has_strip_value_transformer()) {
    if (!filter.strip_value_transformer()) {
      return InvalidArgumentError(
          "`strip_value_transformer` explicitly set to `false`.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    return CellStream(
        [source = std::move(source),
         empty = std::string()]() mutable -> absl::optional<CellView> {
          // We want `empty` to explicitly live for as long as the filter so
          // that the values returned by the filter are valid.
          if (!source) {
            return {};
          }
          auto res = source++;
          res.SetValue(empty);
          return res;
        });
  }
  if (filter.has_chain()) {
    CellStream res = std::move(source);
    // FIXME handle the contexts properly
    for (auto const &subfilter : filter.chain().filters()) {
      if (subfilter.has_sink()) {
        if (!subfilter.sink()) {
          return InvalidArgumentError(
              "`sink` explicitly set to `false`.",
              GCP_ERROR_INFO().WithMetadata("filter", subfilter.DebugString()));
        }
        direct_sinks.emplace_back(std::move(res));
        return CellStream([]() -> absl::optional<CellView> { return {}; });
      }
      auto maybe_res =
          CreateFilterImpl(subfilter, std::move(res), ctx, direct_sinks);
      if (!maybe_res) {
        return maybe_res.status();
      }
      res = *std::move(maybe_res);
    }
    return res;
  }
  if (filter.has_interleave()) {
    std::vector<CellStream> parallel_streams;
    for (auto const & subfilter : filter.interleave().filters()) {
      if (subfilter.has_sink()) {
        if (!subfilter.sink()) {
          return InvalidArgumentError(
              "`sink` explicitly set to `false`.",
              GCP_ERROR_INFO().WithMetadata("filter", subfilter.DebugString()));
        }
        direct_sinks.emplace_back(source);
        continue;
      }
      auto maybe_filter =
          CreateFilterImpl(subfilter, source, ctx, direct_sinks);
      if (!maybe_filter) {
        return maybe_filter.status();
      }
      parallel_streams.emplace_back(*maybe_filter);
    }
    if (parallel_streams.empty()) {
        return CellStream([]() -> absl::optional<CellView> { return {}; });
    }
    return CellStream(MergeCellStreams(parallel_streams));
  }
  if (filter.has_condition()) {
    if (!filter.condition().has_predicate_filter()){
      return InvalidArgumentError(
          "`condition` must have a `predicate_filter` set.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    auto maybe_predicate_stream = CreateFilterImpl(
        filter.condition().predicate_filter(), source, ctx, direct_sinks);
    if (!maybe_predicate_stream) {
      return maybe_predicate_stream.status();
    }
    auto maybe_true_stream =
        filter.condition().has_true_filter()
            ? CreateFilterImpl(filter.condition().true_filter(), source, ctx,
                               direct_sinks)
            : StatusOr<CellStream>(
                  CellStream([]() -> absl::optional<CellView> { return {}; }));
    if (!maybe_true_stream) {
      return maybe_true_stream.status();
    }
    auto maybe_false_stream =
        filter.condition().has_false_filter()
            ? CreateFilterImpl(filter.condition().false_filter(), source, ctx,
                               direct_sinks)
            : StatusOr<CellStream>(
                  CellStream([]() -> absl::optional<CellView> { return {}; }));
    if (!maybe_false_stream) {
      return maybe_true_stream.status();
    }

    return CellStream(ConditionStream(
        std::move(source), *std::move(maybe_predicate_stream),
        *std::move(maybe_true_stream), *std::move(maybe_false_stream)));
  }
  return UnimplementedError(
      "Unsupported filter.",
      GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
}

StatusOr<CellStream> CreateFilter(
    ::google::bigtable::v2::RowFilter const& filter, CellStream source,
    FilterContext const& ctx) {
  std::cout << "Creating a filter structure for: " << std::endl
            << filter.DebugString() << std::endl;
  std::vector<CellStream> direct_sinks;
  if (filter.has_sink()) {
    if (!filter.sink()) {
      return InvalidArgumentError(
          "`sink` explicitly set to `false`.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    return source;
  }
  auto maybe_filter =
      CreateFilterImpl(filter, std::move(source), ctx, direct_sinks);
  if (!maybe_filter) {
    return maybe_filter.status();
  }
  if (!direct_sinks.empty()) {
    direct_sinks.emplace_back(*std::move(maybe_filter));
    return CellStream(MergeCellStreams(std::move(direct_sinks)));
  }
  return maybe_filter;
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
