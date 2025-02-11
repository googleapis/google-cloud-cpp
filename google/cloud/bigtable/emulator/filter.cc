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
#include "google/cloud/bigtable/emulator/range_set.h"
#include "google/cloud/status_or.h"
#include "google/cloud/internal/invoke_result.h"
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

bool PassAllFilters(InternalFilter const&) { return true; }

}  // namespace

FilterContext& FilterContext::DisallowApplyLabel() {
  allow_apply_label_ = false;
  return *this;
}

void CellStream::Next(NextMode mode) {
  if (impl_->Next(mode)) {
    return;
  }
  if (mode == NextMode::kColumn) {
    std::string cur_row_key = impl_->Value().row_key();
    std::string cur_column_family = impl_->Value().column_family();
    std::string cur_column_qualifier = impl_->Value().column_qualifier();
    for (impl_->Next();
         impl_->HasValue() && cur_row_key == impl_->Value().row_key() &&
         cur_column_family == impl_->Value().column_family() &&
         cur_column_qualifier == impl_->Value().column_qualifier();
         impl_->Next());
    return;
  }
  assert(mode == NextMode::kRow);
  std::string cur_row_key = impl_->Value().row_key();
  for (Next(NextMode::kColumn);
       impl_->HasValue() && cur_row_key == impl_->Value().row_key();
       impl_->Next(NextMode::kColumn));
}

template <typename FilterFunctor, typename StateResetFunctor>
class PerRowStateFilter {
  static_assert(
      google::cloud::internal::is_invocable<StateResetFunctor>::value,
      "StateResetFunctor must be invocable with no arguments");
  using State =
      std::decay_t<google::cloud::internal::invoke_result_t<StateResetFunctor>>;
  static_assert(std::is_default_constructible_v<State>,
                "State must be default constructible");
  static_assert(std::is_assignable_v<State&, State>,
                "State must assignable");
  static_assert(std::is_same_v<google::cloud::internal::invoke_result_t<
                                   FilterFunctor, State&, CellView const&>,
                               absl::optional<NextMode>>,
                "Invalid result of `FilterFunctor` invocation.");

 public:
  PerRowStateFilter(FilterFunctor filter, StateResetFunctor reset)
      : filter_(std::move(filter)), reset_(std::move(reset)) {}

  absl::optional<NextMode> operator()(CellView const &cell_view) {
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
  static_assert(
      google::cloud::internal::is_invocable<StateResetFunctor()>::value,
      "StateResetFunctor must be invocable with no arguments");
  using State =
      std::decay_t<google::cloud::internal::invoke_result_t<StateResetFunctor>>;
  static_assert(std::is_default_constructible_v<State>,
                "State must be default constructible");
  static_assert(std::is_assignable_v<State&, State>,
                "State must assignable");
  static_assert(std::is_same_v<google::cloud::internal::invoke_result_t<
                                   FilterFunctor, State&, CellView const&>,
                               absl::optional<NextMode>>,
                "Invali result of `FilterFunctor` invocation.");

 public:
  PerColumnStateFilter(FilterFunctor filter, StateResetFunctor reset)
      : filter_(std::move(filter)), reset_(std::move(reset)) {}

  absl::optional<NextMode> operator()(CellView const &cell_view) {
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

template <class Transformer>
class TrivialTransformer : public AbstractCellStreamImpl {
 public:
  TrivialTransformer(CellStream source, Transformer transformer)
      : source_(std::move(source)), transformer_(std::move(transformer)) {}

  bool ApplyFilter(InternalFilter const& ) override {
    return false;
  }

  bool HasValue() const override { return source_.HasValue(); }

  CellView const& Value() const override {
    if (!transformed_) {
      transformed_ = absl::optional<CellView>(transformer_(source_.Value()));
    }
    return transformed_.value();
  }

  bool Next(NextMode mode) override { 
    source_.Next(mode); 
    transformed_.reset();
    return true;
  }

 private:
  CellStream source_;
  Transformer transformer_;
  mutable absl::optional<CellView> transformed_;
};

template <typename Transformer>
CellStream MakeTrivialTransformer(CellStream source, Transformer transformer) {
  return CellStream(std::make_shared<TrivialTransformer<Transformer>>(
      std::move(source), std::move(transformer)));
}

template <typename Filter>
class TrivialFilter : public AbstractCellStreamImpl {
  static_assert(
      std::is_same_v<
          google::cloud::internal::invoke_result_t<Filter, CellView const&>,
          absl::optional<NextMode>>,
      "Invalid filter return type");

 public:
  TrivialFilter(CellStream source, Filter filter,
                std::function<bool(InternalFilter const&)> filter_filter)
      : initialized_(false),
        source_(std::move(source)),
        filter_(std::move(filter)),
        filter_filter_(std::move(filter_filter)) {}

  bool ApplyFilter(InternalFilter const& filter) override {
    if (filter_filter_(filter)) {
      return source_.ApplyFilter(filter);
    }
    return false;
  }

  bool HasValue() const override {
    InitializeIfNeeded();
    return source_.HasValue();
  }

  CellView const& Value() const override {
    InitializeIfNeeded();
    return source_.Value();
  }

  bool Next(NextMode mode) override {
    source_.Next(mode);
    EnsureCurrentNotFiltered();
    return true;
  }

 private:
  void EnsureCurrentNotFiltered() const {
    while (source_.HasValue()) {
      auto maybe_next_mode = filter_(*source_);
      if (!maybe_next_mode) {
        return;
      }
      source_.Next(*maybe_next_mode);
    }
  }

  void InitializeIfNeeded() const {
    if (!initialized_) {
      EnsureCurrentNotFiltered();
      initialized_ = true;
    }
  }

  mutable bool initialized_;
  mutable CellStream source_;
  mutable Filter filter_;
  std::function<bool(InternalFilter const&)> filter_filter_;
};

template <typename Filter>
CellStream MakeTrivialFilter(
    CellStream source, Filter filter,
    std::function<bool(InternalFilter const&)> filter_filter = PassAllFilters) {
  return CellStream(std::make_shared<TrivialFilter<Filter>>(
      std::move(source), std::move(filter), std::move(filter_filter)));
}

template <typename FilterFunctor, typename StateResetFunctor>
CellStream MakePerRowStateFilter(
    CellStream source, FilterFunctor filter, StateResetFunctor state_reset,
    std::function<bool(InternalFilter const&)> filter_filter = PassAllFilters) {
  return MakeTrivialFilter(std::move(source),
                           PerRowStateFilter<FilterFunctor, StateResetFunctor>(
                               std::move(filter), std::move(state_reset)),
                           std::move(filter_filter));
}

template <typename FilterFunctor, typename StateResetFunctor>
CellStream MakePerColumnStateFilter(
    CellStream source, FilterFunctor filter, StateResetFunctor state_reset,
    std::function<bool(InternalFilter const&)> filter_filter = PassAllFilters) {
  return MakeTrivialFilter(
      std::move(source),
      PerColumnStateFilter<FilterFunctor, StateResetFunctor>(
          std::move(filter), std::move(state_reset)),
      std::move(filter_filter));
}

bool MergeCellStreams::CellStreamGreater::operator()(
    std::unique_ptr<CellStream> const& lhs,
    std::unique_ptr<CellStream> const& rhs) const {
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
  auto col_cmp = internal::CompareColumnQualifiers((*lhs)->column_qualifier(),
                                                   (*rhs)->column_qualifier());
  if (col_cmp != 0) {
    return col_cmp > 0;
  }
  return (*lhs)->timestamp() > (*rhs)->timestamp();
}

MergeCellStreams::MergeCellStreams(std::vector<CellStream> streams)
    : initialized_(false) {
  for (auto& stream : streams) {
    unfinished_streams_.emplace_back(
        std::make_unique<CellStream>(std::move(stream)));
  }
}

bool MergeCellStreams::ApplyFilter(InternalFilter const& internal_filter) {
  assert(!initialized_);
  bool res = true;
  for (auto& stream : unfinished_streams_) {
    res = res && stream->ApplyFilter(internal_filter);
  }
  return res;
}

bool MergeCellStreams::HasValue() const {
  InitializeIfNeeded();
  return !unfinished_streams_.empty();
}

CellView const& MergeCellStreams::Value() const {
  InitializeIfNeeded();
  return unfinished_streams_.front()->Value();
}

bool MergeCellStreams::Next(NextMode mode) {
  InitializeIfNeeded();
  if (unfinished_streams_.empty()) {
    return true;
  }
  if (mode != NextMode::kCell) {
    SkipRowOrColumn(mode);
    return true;
  }
  std::pop_heap(unfinished_streams_.begin(), unfinished_streams_.end(),
                CellStreamGreater());
  auto& stream_to_advance = unfinished_streams_.back();
  stream_to_advance->Next();
  if (stream_to_advance->HasValue()) {
    std::push_heap(unfinished_streams_.begin(), unfinished_streams_.end(),
                   CellStreamGreater());
  } else {
    unfinished_streams_.pop_back();
  }
  return true;
}

void MergeCellStreams::InitializeIfNeeded() const {
  if (!initialized_) {
    ReassesStreams();
    initialized_ = true;
  }
}

void MergeCellStreams::ReassesStreams() const {
  for (auto stream_it = unfinished_streams_.begin();
       stream_it != unfinished_streams_.end(); ++stream_it) {
    if (!(*stream_it)->HasValue()) {
      stream_it->swap(unfinished_streams_.back());
      unfinished_streams_.pop_back();
    }
  }
  std::make_heap(unfinished_streams_.begin(), unfinished_streams_.end(),
                 CellStreamGreater());
}

bool MergeCellStreams::SkipRowOrColumn(NextMode mode) {
  assert(mode != NextMode::kCell);
  // The first element in `unfinished_streams_` is the stream beginning with the
  // smallest Cell - the one we would normally return. Before we alter this
  // stream alter all others which point to the same column/row.
  for (auto stream_it = std::next(unfinished_streams_.begin());
       stream_it != unfinished_streams_.end(); ++stream_it) {
    if ((mode == NextMode::kRow ||
         ((*stream_it)->Value().column_qualifier() ==
              unfinished_streams_.front()->Value().column_qualifier() &&
          (*stream_it)->Value().column_family() ==
              unfinished_streams_.front()->Value().column_family())) &&
        (*stream_it)->Value().row_key() ==
            unfinished_streams_.front()->Value().row_key()) {
      (*stream_it)->Next(mode);
    }
  }
  unfinished_streams_.front()->Next(mode);
  ReassesStreams();
  return true;
}

class ConditionStream : public AbstractCellStreamImpl {
 public:
  ConditionStream(CellStream source, CellStream predicate,
                  CellStream true_stream, CellStream false_stream)
      : source_(std::move(source)),
        predicate_stream_(std::move(predicate)),
        true_stream_(std::move(true_stream)),
        false_stream_(std::move(false_stream)) {}

  bool ApplyFilter(InternalFilter const& ) override {
    return false;
  }

  bool HasValue() const override {
    return true; // FIXME
  }

  CellView const &Value() const override {
    return *source_;  // FIXME
  }

  bool Next(NextMode mode) override {
    if (mode != NextMode::kCell) {
      // FIXME - we can be smarter than that.
      return false;
    }
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
               true_stream_.Next(NextMode::kRow));
        } else {
          // Predicate stream did not return anything for this row.
          condition_true_ = false;
          // Fast-forward the false stream to start at current row.
          for (; false_stream_ &&
                 internal::CompareRowKey(false_stream_->row_key(),
                                         cell_view.row_key()) < 0;
               false_stream_.Next(NextMode::kRow));
        }
      }
      if (*condition_true_) {
        if (true_stream_ && internal::CompareRowKey(true_stream_->row_key(),
                                                    cell_view.row_key()) == 0) {
          return true;
        }
      } else {
        if (false_stream_ &&
            internal::CompareRowKey(false_stream_->row_key(),
                                    cell_view.row_key()) == 0) {
          return true;
        }
      }
      // True/false stream exhausted, reset state and fast-forward source.
      condition_true_.reset();
      for (;
           source_ && internal::CompareRowKey(source_->row_key(),
                                              prev_row_->get()) == 0;
           source_.Next(NextMode::kRow));
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

class EmptyCellStreamImpl : public AbstractCellStreamImpl {
  bool ApplyFilter(InternalFilter const&) override { return true; }
  bool HasValue() const override { return false; }
  CellView const& Value() const override {
    assert(false);
    // The code below makes no sense but it should be dead.
    static CellView dummy{"row", "cf", "col", std::chrono::milliseconds(0),
                          "val"};
    return dummy;
  }
  bool Next(NextMode) override { return true; }
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
    return CellStream(std::make_shared<EmptyCellStreamImpl>());
  }
  if (filter.has_row_key_regex_filter()) {
    auto pattern = std::make_shared<re2::RE2>(filter.row_key_regex_filter());
    if (!pattern->ok()) {
      return InvalidArgumentError(
          "`row_key_regex_filter` is not a valid RE2 regex.",
          GCP_ERROR_INFO()
              .WithMetadata("filter", filter.DebugString())
              .WithMetadata("description", pattern->error()));
    }
    if (source.ApplyFilter(RowKeyRegex{pattern})) {
      return source;
    }
    return MakeTrivialFilter(
        std::move(source),
        [pattern = std::move(pattern)](
            CellView const& cell_view) mutable -> absl::optional<NextMode> {
          if (re2::RE2::PartialMatch(cell_view.row_key(), *pattern)) {
            return {};
          }
          return NextMode::kCell;
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
        [pattern = std::move(pattern)](
            CellView const& cell_view) mutable -> absl::optional<NextMode> {
          if (re2::RE2::PartialMatch(cell_view.value(), *pattern)) {
            return {};
          }
          return NextMode::kCell;
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
        [](bool& should_pass, CellView const&) -> absl::optional<NextMode> {
          if (should_pass) {
            return {};
          }
          return NextMode::kRow;
        },
        [gen = std::mt19937(), pass_prob]() mutable {
          std::uniform_real_distribution<double> dis(0.0, 1.0);
          return dis(gen) < pass_prob;
        });
  }
  if (filter.has_family_name_regex_filter()) {
    auto pattern =
        std::make_shared<re2::RE2>(filter.family_name_regex_filter());
    if (!pattern->ok()) {
      return InvalidArgumentError(
          "`family_name_regex_filter` is not a valid RE2 regex.",
          GCP_ERROR_INFO()
              .WithMetadata("filter", filter.DebugString())
              .WithMetadata("description", pattern->error()));
    }
    if (source.ApplyFilter(FamilyNameRegex{pattern})) {
      return source;
    }
    return MakeTrivialFilter(
        std::move(source),
        [pattern = std::move(pattern)](
            CellView const& cell_view) mutable -> absl::optional<NextMode> {
          if (re2::RE2::PartialMatch(cell_view.column_family(), *pattern)) {
            return {};
          }
          // FIXME we could introduce even column family skipping
          return NextMode::kColumn;
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
    if (source.ApplyFilter(ColumnRegex{pattern})) {
      return source;
    }
    return MakeTrivialFilter(
        std::move(source),
        [pattern = std::move(pattern)](
            CellView const& cell_view) mutable -> absl::optional<NextMode> {
          if (re2::RE2::PartialMatch(cell_view.column_qualifier(), *pattern)) {
            return {};
          }
          return NextMode::kColumn;
        });
  }
  if (filter.has_column_range_filter()) {
    auto maybe_range =
        StringRangeSet::Range::FromColumnRange(filter.column_range_filter());
    if (!maybe_range) {
      return maybe_range.status();
    }
    if (source.ApplyFilter(ColumnRange{*maybe_range})) {
      return source;
    }
    return MakeTrivialFilter(
        std::move(source),
        [range = *std::move(maybe_range),
         column_family = filter.column_range_filter().family_name()](
            CellView const& cell_view) -> absl::optional<NextMode> {
          if ( cell_view.column_family() == column_family &&
                 range.IsWithin(cell_view.column_qualifier())) {
            return {};
          }
          // FIXME - we might know that we should skip the whole column family
          return NextMode::kColumn;
        });
  }
  if (filter.has_value_range_filter()) {
    auto maybe_range =
        StringRangeSet::Range::FromValueRange(filter.value_range_filter());
    if (!maybe_range) {
      return maybe_range.status();
    }
    return MakeTrivialFilter(
        std::move(source),
        [range = *std::move(maybe_range)](
            CellView const& cell_view) -> absl::optional<NextMode> {
          if (range.IsWithin(cell_view.value())) {
            return {};
          }
          return NextMode::kCell;
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
        [](std::int64_t& per_row_state,
           CellView const&) -> absl::optional<NextMode> {
          if (per_row_state-- <= 0) {
            return {};
          }
          return NextMode::kRow;
        },
        [cells_per_row_offset]() { return cells_per_row_offset; },
        [](InternalFilter const& internal_filter) {
          return absl::holds_alternative<RowKeyRegex>(internal_filter);
        });
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
        [cells_per_row_limit](std::int64_t& per_row_state,
                              CellView const&) -> absl::optional<NextMode> {
          if (per_row_state++ < cells_per_row_limit) {
            return {};
          }
          return NextMode::kRow;
        },
        []() -> std::int64_t { return 0; },
        [](InternalFilter const& internal_filter) {
          return absl::holds_alternative<RowKeyRegex>(internal_filter);
        });
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
        [cells_per_column_limit](std::int64_t& per_column_state,
                                 CellView const&) -> absl::optional<NextMode> {
          if (per_column_state++ < cells_per_column_limit) {
            return {};
          }
          return NextMode::kColumn;
        },
        []() -> std::int64_t { return 0; },
        [](InternalFilter const& internal_filter) {
          return !absl::holds_alternative<TimestampRange>(internal_filter);
        });
  }
  if (filter.has_timestamp_range_filter()) {
    auto maybe_range = TimestampRangeSet::Range::FromTimestampRange(
        filter.timestamp_range_filter());
    if (!maybe_range) {
      return maybe_range.status();
    }
    if (source.ApplyFilter(TimestampRange{*maybe_range})) {
      return source;
    }
    return MakeTrivialFilter(
        std::move(source),
        [range = *std::move(maybe_range)](
            CellView const& cell_view) -> absl::optional<NextMode> {
          if (range.IsBelowStart(cell_view.timestamp())) {
            return NextMode::kCell;
          }
          if (range.IsAboveEnd(cell_view.timestamp())) {
            return NextMode::kColumn;
          }
          return {};
        });
  }
  if (filter.has_apply_label_transformer()) {
    if (!ctx.IsApplyLabelAllowed()) {
      return InvalidArgumentError(
          "Two `apply_label_transformer`s cannot coexist in one chain.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    return MakeTrivialTransformer(
        std::move(source),
        [label = std::make_shared<std::string>(
             filter.apply_label_transformer())](CellView cell_view) {
          cell_view.SetLabel(*label);
          return cell_view;
        });
  }
  if (filter.has_strip_value_transformer()) {
    if (!filter.strip_value_transformer()) {
      return InvalidArgumentError(
          "`strip_value_transformer` explicitly set to `false`.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    return MakeTrivialTransformer(std::move(source), [](CellView cell_view) {
      cell_view.SetValue("");
      return cell_view;
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
        return CellStream(std::make_shared<EmptyCellStreamImpl>());
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
        return CellStream(std::make_shared<EmptyCellStreamImpl>());
    }
    return CellStream(std::make_shared<MergeCellStreams>(parallel_streams));
  }
  if (filter.has_condition()) {
    if (!filter.condition().has_predicate_filter()){
      return InvalidArgumentError(
          "`condition` must have a `predicate_filter` set.",
          GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
    }
    // FIXME stream must be deep-copied
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
                  CellStream(std::make_shared<EmptyCellStreamImpl>()));
    if (!maybe_true_stream) {
      return maybe_true_stream.status();
    }
    auto maybe_false_stream =
        filter.condition().has_false_filter()
            ? CreateFilterImpl(filter.condition().false_filter(), source, ctx,
                               direct_sinks)
            : StatusOr<CellStream>(
                  CellStream(std::make_shared<EmptyCellStreamImpl>()));
    if (!maybe_false_stream) {
      return maybe_true_stream.status();
    }

    return CellStream(std::make_shared<ConditionStream>(
        std::move(source), *std::move(maybe_predicate_stream),
        *std::move(maybe_true_stream), *std::move(maybe_false_stream)));
  }
  return UnimplementedError(
      "Unsupported filter.",
      GCP_ERROR_INFO().WithMetadata("filter", filter.DebugString()));
}

CellStream JoinCellStreams(std::vector<CellStream> cell_streams) {
  return CellStream(
      std::make_shared<MergeCellStreams>(std::move(cell_streams)));
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
    return CellStream(
        std::make_shared<MergeCellStreams>(std::move(direct_sinks)));
  }
  return maybe_filter;
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
