// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// [START examples_backtesting_main]
#include "google/cloud/bigtable/cell.h"
#include "google/cloud/bigtable/filters.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/status_or.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "google/protobuf/text_format.h"
#include "strategy.pb.h"
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr char kMinStartDate[] = "2014-01-02";
constexpr char kMaxEndDate[] = "2018-12-31";

constexpr char kRowKeyDelimiter[] = "#";
constexpr char kColumnDelimiter[] = "::";

// Create a namespace alias to make the code easier to read.
namespace cbt = ::google::cloud::bigtable;

// The {start_date, end_date} combination could span multiple years.
std::vector<std::string> PrepareRowKeys(std::string const& ticker,
                                        absl::CivilDay const start_date,
                                        absl::CivilDay const end_date) {
  std::vector<std::string> row_keys;

  // In Bigtable each row is per {ticker + year}.
  auto const start_year = start_date.year();
  auto const end_year = end_date.year();
  for (auto i = start_year; i <= end_year; ++i) {
    row_keys.emplace_back(absl::StrCat(ticker, kRowKeyDelimiter, i));
  }

  return row_keys;
}

cbt::Filter PrepareFilter(cbt::examples::Strategy const& strategy,
                          absl::CivilDay const start_date,
                          absl::CivilDay const end_date) {
  // For one specific row key, we want to query the |base| and |sample| columns
  // specified in strategy, and the timestamp of [start_date, end_date].
  // Use filter combo to implement this.

  // Column filters, only select the targeting columns.
  std::vector<cbt::Filter> column_filters;
  std::set<std::string> existing_signal;
  for (auto const& condition : strategy.conditions()) {
    if (existing_signal.find(condition.base()) == existing_signal.end()) {
      existing_signal.insert(condition.base());
      std::vector<std::string> split =
          absl::StrSplit(condition.base(), kColumnDelimiter);
      // In the format of column_family::column_qualifier.
      if (split.size() != 2) {
        throw std::runtime_error("Invalid strategy definition.");
      }
      column_filters.emplace_back(
          cbt::Filter::ColumnName(split.front(), split.back()));
    }

    if (existing_signal.find(condition.sample()) == existing_signal.end()) {
      existing_signal.insert(condition.sample());
      std::vector<std::string> split =
          absl::StrSplit(condition.sample(), kColumnDelimiter);
      // In the format of column_family::column_qualifier.
      if (split.size() != 2) {
        throw std::runtime_error("Invalid strategy definition.");
      }
      column_filters.emplace_back(
          cbt::Filter::ColumnName(split.front(), split.back()));
    }
  }

  cbt::Filter column_filter = cbt::Filter::InterleaveFromRange(
      column_filters.begin(), column_filters.end());

  // Timestamp filter, only select cells in the target window.
  auto const start_time = absl::FromCivil(start_date, absl::UTCTimeZone());
  auto const end_time = absl::FromCivil(end_date, absl::UTCTimeZone());
  auto const start_milli =
      absl::ToChronoMilliseconds(start_time - absl::UnixEpoch());
  auto const end_milli =
      absl::ToChronoMilliseconds(end_time - absl::UnixEpoch());
  cbt::Filter timestamp_filter =
      cbt::Filter::TimestampRange(start_milli, end_milli);

  return cbt::Filter::Chain(column_filter, timestamp_filter);
}

void CalculateProfit(
    std::unordered_map<std::string, std::map<absl::CivilDay, double>> const&
        time_series,
    cbt::examples::Strategy const& strategy) {
  // Assume the base and sample fields are the same across conditions.
  auto base_it =
      time_series.find(strategy.conditions(0).base())->second.begin();
  auto sample_it =
      time_series.find(strategy.conditions(0).sample())->second.begin();
  ++sample_it;

  double shares = 0.0, wallet = 0.0;
  while (sample_it !=
         time_series.find(strategy.conditions(0).sample())->second.end()) {
    double const sample_price = sample_it->second;
    double const base_price = base_it->second;
    double const change = (sample_price - base_price) / base_price;
    for (auto const& condition : strategy.conditions()) {
      if (condition.threshold() > 0.0 && change > condition.threshold()) {
        // Price goes up; buy in.
        shares += condition.moneyin() / sample_price;
        wallet -= condition.moneyin();
      } else if (condition.threshold() < 0.0 &&
                 change < condition.threshold()) {
        // Pirce goes down; sell out.
        shares -= condition.moneyin() / sample_price;
        wallet += condition.moneyin();
      }
    }

    ++sample_it;
    ++base_it;
  }

  // At the last day of the backtesting period, we evaluate the total value of
  // our investment, and calculate the profit.
  std::cout << "Shares in hand: " << shares << " @ " << base_it->second
            << std::endl;
  std::cout << "Money in hand: " << wallet << std::endl;
  std::cout << "Total profit: " << shares * base_it->second + wallet
            << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 8) {
    std::cerr << "Usage: backtesting <strategy_filepath> <ticker> "
              << "<start_date> <end_date> <project_id> <instance_id> <table_id>"
              << std::endl;
    return 1;
  }

  // Prerequisite check.
  std::string const strategy_filepath = argv[1];
  std::string ticker = argv[2];
  absl::AsciiStrToUpper(&ticker);
  std::string const start_date_str = argv[3];
  std::string const end_date_str = argv[4];
  std::string const project_id = argv[5];
  std::string const instance_id = argv[6];
  std::string const table_id = argv[7];
  if (strategy_filepath.empty() || ticker.empty() || start_date_str.empty() ||
      end_date_str.empty() || project_id.empty() || instance_id.empty() ||
      table_id.empty()) {
    std::cerr << "Please specify necessary parameters." << std::endl;
    return 1;
  }

  absl::CivilDay start_date, end_date, min_start_date, max_end_date;
  if (!absl::ParseCivilTime(start_date_str, &start_date) ||
      !absl::ParseCivilTime(end_date_str, &end_date)) {
    std::cerr << "Can't parse the dates: " << start_date_str << " and "
              << end_date_str << std::endl;
    return 1;
  }
  if (!absl::ParseCivilTime(kMinStartDate, &min_start_date) ||
      !absl::ParseCivilTime(kMaxEndDate, &max_end_date)) {
    std::cerr << "Can't parse the backtesting window." << std::endl;
    return 1;
  }
  if (start_date < min_start_date || end_date > max_end_date ||
      start_date >= end_date) {
    std::cerr << "Backtesting only supports time window "
              << "[2016-10-31, 2021-10-29).";
    return 1;
  }

  // Read the input strategy file.
  std::fstream input_file(strategy_filepath);
  if (!input_file.is_open()) {
    std::cerr << "Error in opening file: " << strategy_filepath << std::endl;
    return 1;
  }
  std::string input_str(std::istreambuf_iterator<char>{input_file}, {});
  cbt::examples::Strategy strategy;
  if (!google::protobuf::TextFormat::ParseFromString(input_str, &strategy)) {
    std::cerr << "Can't parse the input strategy." << std::endl;
    return 1;
  }

  // Prepare the table.
  cbt::Table table(cbt::MakeDataClient(project_id, instance_id), table_id);

  auto const row_keys = PrepareRowKeys(ticker, start_date, end_date);
  auto const filter = PrepareFilter(strategy, start_date, end_date);

  // A map from column_name/signal to {date, price} map;
  // Note the inner map is sorted by date.
  std::unordered_map<std::string, std::map<absl::CivilDay, double>> signal_map;

  for (auto const& row_key : row_keys) {
    auto result = table.ReadRow(row_key, filter).value();
    if (!result.first) {
      std::cerr << "Error in reading row key: " << row_key << "; continue."
                << std::endl;
      continue;
    }

    auto const& cells = result.second.cells();
    for (auto const& cell : cells) {
      absl::Duration const ts_duration = absl::FromChrono(cell.timestamp());
      absl::Time const ts_time =
          absl::time_internal::FromUnixDuration(ts_duration);
      absl::CivilDay const ts_cd =
          absl::ToCivilDay(ts_time, absl::UTCTimeZone());

      double price_value;
      if (!absl::SimpleAtod(cell.value(), &price_value)) {
        std::cerr << "Can't parse the cell value: " << cell.value()
                  << std::endl;
        continue;
      }

      signal_map[absl::StrCat(cell.family_name(), kColumnDelimiter,
                              cell.column_qualifier())]
          .emplace(ts_cd, price_value);
    }
  }

  CalculateProfit(signal_map, strategy);

  return 0;
}
// [END examples_backtesting_main]
