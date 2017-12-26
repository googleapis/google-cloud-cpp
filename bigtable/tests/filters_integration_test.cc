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
#include <sstream>
#include "bigtable/admin/admin_client.h"
#include "bigtable/admin/table_admin.h"
#include "bigtable/client/cell.h"
#include "bigtable/client/data_client.h"
#include "bigtable/client/table.h"
#include "bigtable/client/filters.h"

namespace btproto = ::google::bigtable::v2;

namespace {
// TODO(#32) - change the function signature when Table::ReadRows() is a thing.
// All these functions would become:
//     `Function(bigtable::Table& table, std::string const& row_key)`
//     `Function(bigtable::Table& table, std::string const& begin,
//               std::string const& end)`
//
void CheckPassAll(bigtable::DataClient& client, bigtable::Table& table,
                  std::string const& row_key);
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

  return 0;
} catch (bigtable::PermanentMutationFailure const& ex) {
  std::cerr << "bigtable::PermanentMutationFailure raised: " << ex.what()
            << " - " << ex.status().error_message() << " ["
            << ex.status().error_code()
            << "], details=" << ex.status().error_details() << std::endl;
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}

namespace {
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
  std::cout << "CheckPassAll() is sucessful" << std::endl;
}
}  // anonymous namespace
