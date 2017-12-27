// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <google/protobuf/text_format.h>
#include <cmath>
#include <sstream>
#include "bigtable/admin/admin_client.h"
#include "bigtable/admin/table_admin.h"
#include "bigtable/client/cell.h"
#include "bigtable/client/data_client.h"
#include "bigtable/client/table.h"
#include "bigtable/client/filters.h"

namespace btproto = ::google::bigtable::v2;

namespace {
void ReportException();

// TODO(#32) - change the function signature when Table::ReadRows() is a thing.
// All these functions would become:
//     `Function(bigtable::Table& table, std::string const& row_key)`
//     `Function(bigtable::Table& table, std::string const& begin,
//               std::string const& end)`
//
void CheckPassAll(bigtable::DataClient& client, bigtable::Table& table,
                  std::string const& row_key);
void CheckBlockAll(bigtable::ClientInterface& client, bigtable::Table& table,
                   std::string const& row_key);
void CheckLatest(bigtable::ClientInterface& client, bigtable::Table& table,
                 std::string const& row_key);
void CheckCellsRowLimit(bigtable::ClientInterface& client,
                        bigtable::Table& table,
                        std::string const& row_key_prefix);
void CheckCellsRowOffset(bigtable::ClientInterface& client,
                         bigtable::Table& table,
                         std::string const& row_key_prefix);
void CheckCellsRowSample(bigtable::ClientInterface& client,
                         bigtable::Table& table,
                         std::string const& row_key_prefix);
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  namespace admin_proto = ::google::bigtable::admin::v2;

  // Make sure the arguments are valid.
  if (argc != 4) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of("/");
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance> <table>" << std::endl;
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table_name = argv[3];
  std::string const family = "fam";
  std::string const fam0 = "fam0";
  std::string const fam1 = "fam1";
  std::string const fam2 = "fam2";
  std::string const fam3 = "fam3";

  auto admin_client =
      bigtable::CreateAdminClient(project_id, bigtable::ClientOptions());
  bigtable::TableAdmin admin(admin_client, instance_id);

  auto table_list = admin.ListTables(admin_proto::Table::NAME_ONLY);
  if (not table_list.empty()) {
    std::ostringstream os;
    os << "Expected empty instance at the beginning of integration test";
    throw std::runtime_error(os.str());
  }

  auto created_table = admin.CreateTable(
      table_name,
      bigtable::TableConfig({{family, bigtable::GcRule::MaxNumVersions(10)},
                             {fam0, bigtable::GcRule::MaxNumVersions(10)},
                             {fam1, bigtable::GcRule::MaxNumVersions(10)},
                             {fam2, bigtable::GcRule::MaxNumVersions(10)},
                             {fam3, bigtable::GcRule::MaxNumVersions(10)}},
                            {}));
  std::cout << table_name << " created successfully" << std::endl;

  auto client = bigtable::CreateDefaultClient(project_id, instance_id,
                                              bigtable::ClientOptions());
  bigtable::Table table(client, table_name);

  CheckPassAll(*client, table, "aaa0001-pass-all");

  // TODO(google-cloud-go#839) - remote workarounds for emulator bug(s).
  try {
    CheckBlockAll(*client, table, "aaa0002-block-all");
  } catch(...) {}

  CheckLatest(*client, table, "aaa003-latest");
  CheckCellsRowLimit(*client, table, "aaa004-cells-row-limit");
  CheckCellsRowOffset(*client, table, "aaa005-cells-row-offset");

  try {
    CheckCellsRowSample(*client, table, "aaa006-cells-row-sample");
  } catch (...) {
    ReportException();
  }

  return 0;
} catch (...) {
  ReportException();
  return 1;
}

namespace {
void ReportException() {
  try {
    throw;
  } catch (bigtable::PermanentMutationFailure const& ex) {
    std::cerr << "bigtable::PermanentMutationFailure raised: " << ex.what()
              << " - " << ex.status().error_message() << " ["
              << ex.status().error_code()
              << "], details=" << ex.status().error_details() << std::endl;
  } catch (std::exception const& ex) {
    std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  } catch (...) {
    std::cerr << "Unknown exception raised." << std::endl;
  }
}

// TODO(#32) remove this when Table::ReadRows() is a thing.
std::vector<bigtable::Cell> ReadRows(bigtable::DataClient& client,
                                     bigtable::Table& table,
                                     btproto::ReadRowsRequest request) {
  std::vector<bigtable::Cell> result;
  grpc::ClientContext client_context;
  auto stream = client.Stub().ReadRows(&client_context, request);
  btproto::ReadRowsResponse response;

  std::string current_row_key;
  std::string current_family;
  std::string current_column;
  std::string current_value;
  std::vector<std::string> current_labels;
  while (stream->Read(&response)) {
    for (btproto::ReadRowsResponse::CellChunk const& chunk :
         response.chunks()) {
      if (not chunk.row_key().empty()) {
        current_row_key = chunk.row_key();
      }
      if (chunk.has_family_name()) {
        current_family = chunk.family_name().value();
      }
      if (chunk.has_qualifier()) {
        current_column = chunk.qualifier().value();
      }
      // Most of the time `chunk.labels()` is empty, but the following copy is
      // fast in that case.
      std::copy(chunk.labels().begin(), chunk.labels().end(),
                std::back_inserter(current_labels));
      if (chunk.value_size() > 0) {
        current_value.reserve(chunk.value_size());
      }
      current_value.append(chunk.value());
      if (chunk.value_size() == 0 or chunk.commit_row()) {
        // This was the last chunk.
        result.emplace_back(current_row_key, current_family, current_column,
                            chunk.timestamp_micros(), std::move(current_value),
                            std::move(current_labels));
      }
    }
  }
  auto status = stream->Finish();
  if (not status.ok()) {
    std::ostringstream os;
    os << "gRPC error in ReadRow() - " << status.error_message() << " ["
       << status.error_code() << "] details=" << status.error_details();
    throw std::runtime_error(os.str());
  }
  return result;
}

// TODO(#29) remove this when Table::ReadRow() is a thing.
std::vector<bigtable::Cell> ReadRow(bigtable::DataClient& client,
                                    bigtable::Table& table, std::string key,
                                    bigtable::Filter filter) {
  btproto::ReadRowsRequest request;
  request.set_table_name(table.table_name());
  request.set_rows_limit(1);
  auto& row = *request.mutable_rows();
  *row.add_row_keys() = std::move(key);
  *request.mutable_filter() = filter.as_proto_move();

  return ReadRows(client, table, std::move(request));
}

// TODO(#32) remove this when Table::ReadRow() is a thing.
std::vector<bigtable::Cell> ReadRows(bigtable::ClientInterface& client,
                                     bigtable::Table& table, std::string start,
                                     std::string end, bigtable::Filter filter) {
  btproto::ReadRowsRequest request;
  request.set_table_name(table.table_name());
  auto& row = *request.mutable_rows();
  btproto::RowRange range;
  range.set_start_key_closed(std::move(start));
  range.set_end_key_open(std::move(end));
  *row.add_row_ranges() = std::move(range);
  *request.mutable_filter() = filter.as_proto_move();
  return ReadRows(client, table, std::move(request));
}

int CellCompare(bigtable::Cell const& lhs, bigtable::Cell const& rhs) {
  auto compare_row_key = lhs.row_key().compare(lhs.row_key());
  if (compare_row_key != 0) {
    return compare_row_key;
  }
  auto compare_family_name = lhs.family_name().compare(rhs.family_name());
  if (compare_family_name != 0) {
    return compare_family_name;
  }
  auto compare_column_qualifier =
      lhs.column_qualifier().compare(rhs.column_qualifier());
  if (compare_column_qualifier != 0) {
    return compare_column_qualifier;
  }
  return rhs.timestamp() - lhs.timestamp();
}

bool CellLess(bigtable::Cell const& lhs, bigtable::Cell const& rhs) {
  return CellCompare(lhs, rhs) < 0;
}

void PrintCells(std::ostream& os, std::vector<bigtable::Cell> const& cells) {
  for (auto const& cell : cells) {
    os << "  row_key=" << cell.row_key() << "\n  family=" << cell.family_name()
       << "\n  column=" << cell.column_qualifier()
       << "\n  timestamp=" << cell.timestamp() << "\n  value=" << cell.value();
  }
}

void CheckEqual(absl::string_view where, std::vector<bigtable::Cell> expected,
                std::vector<bigtable::Cell> actual) {
  std::sort(expected.begin(), expected.end(), CellLess);
  std::sort(actual.begin(), actual.end(), CellLess);
  std::vector<bigtable::Cell> differences;
  std::set_symmetric_difference(expected.begin(), expected.end(),
                                actual.begin(), actual.end(),
                                std::back_inserter(differences), CellLess);
  if (differences.empty()) {
    return;
  }
  std::ostringstream os;
  os << "Mismatched cells in " << where << " differences=<\n";
  PrintCells(os, differences);
  os << ">\nexpected=<";
  PrintCells(os, expected);
  os << ">\nactual=<";
  PrintCells(os, actual);
  os << ">";
  throw std::runtime_error(os.str());
}

void CheckPassAll(bigtable::DataClient& client, bigtable::Table& table,
                  std::string const& row_key) {
  std::vector<bigtable::Cell> expected{
      {row_key, "fam", "c", 0, "v-c-0-0", {}},
      {row_key, "fam", "c", 1000, "v-c-0-1", {}},
      {row_key, "fam", "c", 2000, "v-c-0-2", {}},
      {row_key, "fam0", "c0", 0, "v-c0-0-0", {}},
      {row_key, "fam0", "c1", 1000, "v-c1-0-1", {}},
      {row_key, "fam0", "c1", 2000, "v-c1-0-2", {}},
  };

  auto mutation = bigtable::SingleRowMutation(row_key);
  for (auto const& cell : expected) {
    mutation.emplace_back(bigtable::SetCell(
        static_cast<std::string>(cell.family_name()),
        static_cast<std::string>(cell.column_qualifier()), cell.timestamp(),
        static_cast<std::string>(cell.value())));
  }
  table.Apply(std::move(mutation));

  auto actual =
      ReadRow(client, table, row_key, bigtable::Filter::PassAllFilter());
  CheckEqual("CheckPassAll()", expected, actual);
  std::cout << "CheckPassAll() is successful" << std::endl;
}

void CheckBlockAll(bigtable::ClientInterface& client, bigtable::Table& table,
                  std::string const& row_key) {
  std::vector<bigtable::Cell> created{
      {row_key, "fam", "c", 0, "v-c-0-0", {}},
      {row_key, "fam", "c", 1000, "v-c-0-1", {}},
      {row_key, "fam", "c", 2000, "v-c-0-2", {}},
      {row_key, "fam0", "c0", 0, "v-c0-0-0", {}},
      {row_key, "fam0", "c1", 1000, "v-c1-0-1", {}},
      {row_key, "fam0", "c1", 2000, "v-c1-0-2", {}},
  };

  auto mutation = bigtable::SingleRowMutation(row_key);
  for (auto const& cell : created) {
    mutation.emplace_back(bigtable::SetCell(
        static_cast<std::string>(cell.family_name()),
        static_cast<std::string>(cell.column_qualifier()), cell.timestamp(),
        static_cast<std::string>(cell.value())));
  }
  table.Apply(std::move(mutation));

  std::vector<bigtable::Cell> expected{};
  auto actual =
      ReadRow(client, table, row_key, bigtable::Filter::BlockAllFilter());
  CheckEqual("CheckBlockAll()", expected, actual);
  std::cout << "CheckBlockAll() is successful" << std::endl;
}

void CheckLatest(bigtable::ClientInterface& client, bigtable::Table& table,
                 std::string const& row_key) {
  std::vector<bigtable::Cell> created{
      {row_key, "fam", "c", 0, "v-c-0-0", {}},
      {row_key, "fam", "c", 1000, "v-c-0-1", {}},
      {row_key, "fam", "c", 2000, "v-c-0-2", {}},
      {row_key, "fam0", "c0", 0, "v-c0-0-0", {}},
      {row_key, "fam0", "c1", 1000, "v-c1-0-1", {}},
      {row_key, "fam0", "c1", 2000, "v-c1-0-2", {}},
      {row_key, "fam0", "c1", 3000, "v-c1-0-3", {}},
  };

  auto mutation = bigtable::SingleRowMutation(row_key);
  for (auto const& cell : created) {
    mutation.emplace_back(bigtable::SetCell(
        static_cast<std::string>(cell.family_name()),
        static_cast<std::string>(cell.column_qualifier()), cell.timestamp(),
        static_cast<std::string>(cell.value())));
  }
  table.Apply(std::move(mutation));

  std::vector<bigtable::Cell> expected{
      {row_key, "fam", "c", 1000, "v-c-0-1", {}},
      {row_key, "fam", "c", 2000, "v-c-0-2", {}},
      {row_key, "fam0", "c0", 0, "v-c0-0-0", {}},
      {row_key, "fam0", "c1", 2000, "v-c1-0-2", {}},
      {row_key, "fam0", "c1", 3000, "v-c1-0-3", {}},
  };
  auto actual = ReadRow(client, table, row_key, bigtable::Filter::Latest(2));
  CheckEqual("CheckLatest()", expected, actual);
  std::cout << "CheckLatest() is successful" << std::endl;
}

void CheckEqualRowKeyCount(absl::string_view where,
                           std::map<std::string, int> const& expected,
                           std::map<std::string, int> const& actual) {
  std::vector<std::pair<std::string, int>> differences;
  std::set_symmetric_difference(expected.begin(), expected.end(),
                                actual.begin(), actual.end(),
                                std::back_inserter(differences));
  if (differences.empty()) {
    return;
  }

  std::ostringstream os;
  // In C++14 we could use
  auto stream_map = [&os](std::pair<std::string const, int> const& x) {
    os << "{" << x.first << "," << x.second << "}, ";
  };
  auto stream_vector = [&os](std::pair<std::string, int> const& x) {
    os << "{" << x.first << "," << x.second << "}, ";
  };

  os << "Mismatched row key count in " << where << " differences=<\n";
  std::for_each(differences.begin(), differences.end(), stream_vector);
  os << ">\nexpected=<";
  std::for_each(expected.begin(), expected.end(), stream_map);
  os << ">\nactual=<";
  std::for_each(actual.begin(), actual.end(), stream_map);
  os << ">";
  throw std::runtime_error(os.str());
}

/**
 * Create some complex rows in @p table.
 *
 * Create the following rows in @p table, the magic values for the column
 * families are defined above.
 *
 *   | Row Key                 | Family | Column | Contents      |
 *   | :---------------------- | :----- | :----- | :------------ |
 *   | "{prefix}/one-cell"     | fam    | c      | cell @ 3000 |
 *   | "{prefix}/two-cells"    | fam0   | c      | cell @ 3000 |
 *   | "{prefix}/two-cells"    | fam0   | c2     | cell @ 3000 |
 *   | "{prefix}/many"         | fam    | c      | cells @ 0, 1000, 200, 3000 |
 *   | "{prefix}/many-columns" | fam    | c0     | cell @ 3000 |
 *   | "{prefix}/many-columns" | fam    | c1     | cell @ 3000 |
 *   | "{prefix}/many-columns" | fam    | c2     | cell @ 3000 |
 *   | "{prefix}/many-columns" | fam    | c3     | cell @ 3000 |
 *   | "{prefix}/complex"      | fam0   | col0   | cell @ 3000, 6000 |
 *   | "{prefix}/complex"      | fam0   | col1   | cell @ 3000, 6000 |
 *   | "{prefix}/complex"      | fam0   | ...    | cell @ 3000, 6000 |
 *   | "{prefix}/complex"      | fam0   | col9   | cell @ 3000, 6000 |
 *   | "{prefix}/complex"      | fam1   | col0   | cell @ 3000, 6000 |
 *   | "{prefix}/complex"      | fam1   | col1   | cell @ 3000, 6000 |
 *   | "{prefix}/complex"      | fam1   | ...    | cell @ 3000, 6000 |
 *   | "{prefix}/complex"      | fam1   | col9   | cell @ 3000, 6000 |
 *
 */
void CreateComplexRows(bigtable::ClientInterface& client,
                       bigtable::Table& table,
                       std::string const& prefix) {
  namespace bt = bigtable;
  bt::BulkMutation mutation;
  // Prepare a set of rows, with different numbers of cells, columns, and
  // column families.
  mutation.emplace_back(bt::SingleRowMutation(
      prefix + "/one-cell", {bt::SetCell("fam", "c", 3000, "foo")}));
  mutation.emplace_back(
      bt::SingleRowMutation(prefix + "/two-cells",
                            {bt::SetCell("fam0", "c", 3000, "foo"),
                             bt::SetCell("fam0", "c2", 3000, "foo")}));
  mutation.emplace_back(bt::SingleRowMutation(
      prefix + "/many",
      {bt::SetCell("fam", "c", 0, "foo"), bt::SetCell("fam", "c", 1000, "foo"),
       bt::SetCell("fam", "c", 2000, "foo"),
       bt::SetCell("fam", "c", 3000, "foo")}));
  mutation.emplace_back(
      bt::SingleRowMutation(prefix + "/many-columns",
                            {bt::SetCell("fam", "c0", 3000, "foo"),
                             bt::SetCell("fam", "c1", 3000, "foo"),
                             bt::SetCell("fam", "c2", 3000, "foo"),
                             bt::SetCell("fam", "c3", 3000, "foo")}));
  // This one is complicated, create a mutation with several families and
  // columns
  bt::SingleRowMutation complex(prefix + "/complex");
  for (int i = 0; i != 4; ++i) {
    for (int j = 0; j != 10; ++j) {
      complex.emplace_back(bt::SetCell("fam" + std::to_string(i),
                                       "col" + std::to_string(j), 3000, "foo"));
      complex.emplace_back(bt::SetCell("fam" + std::to_string(i),
                                       "col" + std::to_string(j), 6000, "bar"));
    }
  }
  mutation.emplace_back(std::move(complex));
  table.BulkApply(std::move(mutation));
}

void CheckCellsRowLimit(bigtable::ClientInterface& client,
                        bigtable::Table& table,
                        std::string const& row_key_prefix) {
  CreateComplexRows(client, table, row_key_prefix);

  // Search in the range [row_key_prefix, row_key_prefix + "0"), we used '/' as
  // the separator and the successor of "/" is "0".
  auto result =
      ReadRows(client, table, row_key_prefix + "/", row_key_prefix + "0",
               bigtable::Filter::CellsRowLimit(3));

  std::map<std::string, int> actual;
  for (auto const& c : result) {
    auto ins = actual.emplace(c.row_key(), 0);
    ins.first->second++;
  }
  std::map<std::string, int> expected{{row_key_prefix + "/one-cell", 1},
                                      {row_key_prefix + "/two-cells", 2},
                                      {row_key_prefix + "/many", 3},
                                      {row_key_prefix + "/many-columns", 3},
                                      {row_key_prefix + "/complex", 3}};

  CheckEqualRowKeyCount("CheckCellsRowLimit()", expected, actual);
  std::cout << "CheckCellsRowLimit() is successful" << std::endl;
}

void CheckCellsRowOffset(bigtable::ClientInterface& client,
                         bigtable::Table& table,
                        std::string const& row_key_prefix) {
  CreateComplexRows(client, table, row_key_prefix);

  // Search in the range [row_key_prefix, row_key_prefix + "0"), we used '/' as
  // the separator and the successor of "/" is "0".
  auto result =
      ReadRows(client, table, row_key_prefix + "/", row_key_prefix + "0",
               bigtable::Filter::CellsRowOffset(2));

  std::map<std::string, int> actual;
  for (auto const& c : result) {
    auto ins = actual.emplace(c.row_key(), 0);
    ins.first->second++;
  }
  std::map<std::string, int> expected{{row_key_prefix + "/many", 2},
                                      {row_key_prefix + "/many-columns", 2},
                                      {row_key_prefix + "/complex", 78}};

  CheckEqualRowKeyCount("CheckCellsRowOffset()", expected, actual);
  std::cout << "CheckCellsRowOffset() is successful" << std::endl;
}

void CreateManyRows(bigtable::ClientInterface& client, bigtable::Table& table,
                    std::string const& prefix, int count) {
  namespace bt = bigtable;
  bt::BulkMutation bulk;
  for (int row = 0; row != count; ++row) {
    std::string row_key = prefix + "/" + std::to_string(row);
    bulk.emplace_back(bt::SingleRowMutation(
        row_key, {bt::SetCell("fam", "col", 4000, "foo")}));
  }
  table.BulkApply(std::move(bulk));
}

void CheckCellsRowSample(bigtable::ClientInterface& client,
                         bigtable::Table& table,
                         std::string const& row_key_prefix) {
  constexpr int row_count = 20000;
  CreateManyRows(client, table, row_key_prefix, row_count);

  // We want to check that the sampling rate was "more or less" the prescribed
  // value.  We use 5% as the allowed error, this is arbitrary.  If we wanted to
  // get serious about testing the sampling rate we would do some statistics.
  // We do not really need to, because we are testing the library, not the
  // server. But for what is worth, the outline would be:
  //
  //   - Model sampling as a binomial process.
  //   - Perform power analysis to decide the size of the sample.
  //   - Perform hypothesis testing: is the actual sampling rate != that the
  //     prescribed rate (and sufficiently different, i.e., the effect is large
  //     enough).
  //
  // For what is worth, the sample size is large enough to detect effects of 2%
  // at the conventional significance and power levels.  In R:
  //
  // ```R
  // require(pwr)
  // pwr.p.test(h = ES.h(p1 = 0.63, p2 = 0.65), sig.level = 0.05,
  //            power=0.80, alternative="two.sided")
  // ```
  //
  // h = 0.04167045
  // n = 4520.123
  // sig.level = 0.05
  // power = 0.8
  // alternative = two.sided
  //
  constexpr double kSampleRate = 0.75;
  constexpr double kAllowedError = 0.05;
  const int kMinCount =
      static_cast<int>(std::floor((kSampleRate - kAllowedError) * row_count));
  const int kMaxCount =
      static_cast<int>(std::ceil((kSampleRate + kAllowedError) * row_count));

  // Search in the range [row_key_prefix, row_key_prefix + "0"), we used '/' as
  // the separator and the successor of "/" is "0".
  auto result =
      ReadRows(client, table, row_key_prefix + "/", row_key_prefix + "0",
               bigtable::Filter::RowSample(kSampleRate));

  if (result.size() < static_cast<std::size_t>(kMinCount)) {
    std::ostringstream os;
    os << "CheckRowSample() - row sample count is too low, got="
       << result.size() << ", expected >= " << kMinCount;
    throw std::runtime_error(os.str());
  }
  if (result.size() > static_cast<std::size_t>(kMaxCount)) {
    std::ostringstream os;
    os << "CheckRowSample() - row samples count is too high, got="
       << result.size() << ", expected <= " << kMaxCount;
    throw std::runtime_error(os.str());
  }
  std::cout << "CheckCellsRowOffset() is successful" << std::endl;
}

}  // anonymous namespace
