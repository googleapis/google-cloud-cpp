#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <google/bigtable/v2/data.pb.h>
#include <absl/strings/str_format.h>
#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

struct SetCellParams {
  std::string column_family_name;
  std::string column_qualifier;
  int64_t timestamp_micros;
  std::string data;
};

StatusOr<std::shared_ptr<Table>> CreateTable(
    std::string const& table_name, std::vector<std::string>& column_families) {
  ::google::bigtable::admin::v2::Table schema;
  schema.set_name(table_name);
  for (auto& column_family_name : column_families) {
    (*schema.mutable_column_families())[column_family_name] =
        ::google::bigtable::admin::v2::ColumnFamily();
  }

  return Table::Create(schema);
}

Status HasCell(std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
               std::string const& column_family, std::string const& row_key,
               std::string const& column_qualifier, int64_t timestamp_micros,
               std::string const& value) {
  auto column_family_it = table->find(column_family);
  if (column_family_it == table->end()) {
    return NotFoundError(
        "column family not found in table",
        GCP_ERROR_INFO().WithMetadata("column family", column_family));
  }

  auto const& cf = column_family_it->second;
  auto column_family_row_it = cf->find(row_key);
  if (column_family_row_it == cf->end()) {
    return NotFoundError("no row key found in column family",
                         GCP_ERROR_INFO()
                             .WithMetadata("row key", row_key)
                             .WithMetadata("column family", column_family));
  }

  auto& column_family_row = column_family_row_it->second;
  auto column_row_it = column_family_row.find(column_qualifier);
  if (column_row_it == column_family_row.end()) {
    return NotFoundError(
        "no column found with qualifier",
        GCP_ERROR_INFO().WithMetadata("column qualifier", column_qualifier));
  }

  auto& column_row = column_row_it->second;
  auto timestamp_it =
      column_row.find(std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::microseconds(timestamp_micros)));
  if (timestamp_it == column_row.end()) {
    return NotFoundError(
        "timestamp not found",
        GCP_ERROR_INFO().WithMetadata("timestamp",
                                      absl::StrFormat("%d", timestamp_micros)));
  }

  if (timestamp_it->second != value) {
    return NotFoundError("wrong value",
                         GCP_ERROR_INFO()
                             .WithMetadata("expected", value)
                             .WithMetadata("found", timestamp_it->second));
  }

  return Status();
}

Status SetCells(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& table_name, std::string const& row_key,
    std::vector<SetCellParams>& set_cell_params) {
  ::google::bigtable::v2::MutateRowRequest mutation_request;
  mutation_request.set_table_name(table_name);
  mutation_request.set_row_key(row_key);

  for (auto m : set_cell_params) {
    auto* mutation_request_mutation = mutation_request.add_mutations();
    auto* set_cell_mutation = mutation_request_mutation->mutable_set_cell();
    set_cell_mutation->set_family_name(m.column_family_name);
    set_cell_mutation->set_column_qualifier(m.column_qualifier);
    set_cell_mutation->set_timestamp_micros(m.timestamp_micros);
    set_cell_mutation->set_value(m.data);
  }

  return table->MutateRow(mutation_request);
}

TEST(ConditionalMutations, TestTrueMutations) {
  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const column_family_name = "test_column_family";
  auto const* const row_key = "0";
  auto const* const column_qualifier = "column_1";
  auto timestamp_micros = 1000;
  auto const* const true_mutation_value = "set by a true mutation";
  auto const* const false_mutation_value = "set by a false mutation";

  std::vector<std::string> column_families = {column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  ::google::bigtable::v2::Mutation true_mutation;
  auto* set_cell_mutation = true_mutation.mutable_set_cell();
  set_cell_mutation->set_family_name(column_family_name);
  set_cell_mutation->set_column_qualifier(column_qualifier);
  set_cell_mutation->set_timestamp_micros(timestamp_micros);
  set_cell_mutation->set_value(true_mutation_value);

  std::vector<google::bigtable::v2::Mutation> true_mutations = {true_mutation};

  ::google::bigtable::v2::Mutation false_mutation;
  set_cell_mutation = false_mutation.mutable_set_cell();
  set_cell_mutation->set_family_name(column_family_name);
  set_cell_mutation->set_column_qualifier(column_qualifier);
  set_cell_mutation->set_timestamp_micros(timestamp_micros);
  set_cell_mutation->set_value(false_mutation_value);

  std::vector<google::bigtable::v2::Mutation> false_mutations = {
      false_mutation};

  std::vector<SetCellParams> v = {
      {column_family_name, "column_2", 1000, "some_value"}};
  ASSERT_STATUS_OK(SetCells(table, table_name, row_key, v));
  ASSERT_STATUS_OK(HasCell(table, v[0].column_family_name, row_key,
                           v[0].column_qualifier, v[0].timestamp_micros,
                           v[0].data));

  google::bigtable::v2::CheckAndMutateRowRequest cond_mut_with_pass_all;

  cond_mut_with_pass_all.set_row_key(row_key);
  cond_mut_with_pass_all.set_table_name(table_name);
  cond_mut_with_pass_all.mutable_predicate_filter()->set_pass_all_filter(true);
  cond_mut_with_pass_all.mutable_true_mutations()->Assign(
      true_mutations.begin(), true_mutations.end());
  cond_mut_with_pass_all.mutable_false_mutations()->Assign(
      false_mutations.begin(), false_mutations.end());

  auto status_or = table->CheckAndMutateRow(cond_mut_with_pass_all);
  ASSERT_STATUS_OK(status_or);

  // pass_all_filter means that true_mutation should have succeeded,
  // so check for the true_mutation cell value e.t.c.
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           timestamp_micros, true_mutation_value));

  // And just for good measure, ensure that false_mutation was not written.
  ASSERT_EQ(false, HasCell(table, column_family_name, row_key, column_qualifier,
                           timestamp_micros, false_mutation_value)
                       .ok());
}

TEST(ConditionalMutations, RejectInvalidRequest) {
  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const column_family_name = "test_column_family";
  auto const* const row_key = "0";
  auto const* const column_qualifier = "column_1";
  auto timestamp_micros = 1000;
  auto const* const true_mutation_value = "set by a true mutation";
  auto const* const false_mutation_value = "set by a false mutation";

  std::vector<std::string> column_families = {column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  ::google::bigtable::v2::Mutation true_mutation;
  auto* set_cell_mutation = true_mutation.mutable_set_cell();
  set_cell_mutation->set_family_name(column_family_name);
  set_cell_mutation->set_column_qualifier(column_qualifier);
  set_cell_mutation->set_timestamp_micros(timestamp_micros);
  set_cell_mutation->set_value(true_mutation_value);

  std::vector<google::bigtable::v2::Mutation> true_mutations = {true_mutation};

  ::google::bigtable::v2::Mutation false_mutation;
  set_cell_mutation = false_mutation.mutable_set_cell();
  set_cell_mutation->set_family_name(column_family_name);
  set_cell_mutation->set_column_qualifier(column_qualifier);
  set_cell_mutation->set_timestamp_micros(timestamp_micros);
  set_cell_mutation->set_value(false_mutation_value);

  // Will be configured so that row_key is not set.
  std::vector<google::bigtable::v2::Mutation> false_mutations = {
      false_mutation};

  google::bigtable::v2::CheckAndMutateRowRequest cond_mutation_no_row_key;

  cond_mutation_no_row_key.set_table_name(table_name);
  cond_mutation_no_row_key.mutable_true_mutations()->Assign(
      true_mutations.begin(), true_mutations.end());
  cond_mutation_no_row_key.mutable_false_mutations()->Assign(
      false_mutations.begin(), false_mutations.end());

  auto status_or = table->CheckAndMutateRow(cond_mutation_no_row_key);
  ASSERT_EQ(false, status_or.ok());

  // Will be configured so that both true_mutations and
  // false_mutations are empty.
  google::bigtable::v2::CheckAndMutateRowRequest cond_mutation_no_mutations;
  cond_mutation_no_mutations.set_row_key(row_key);
  cond_mutation_no_row_key.set_table_name(table_name);
  ASSERT_EQ(false, table->CheckAndMutateRow(cond_mutation_no_mutations).ok());
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
