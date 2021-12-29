// Parse the input csv file, and write the data into the BigTable.

#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/mutations.h"
#include "google/cloud/bigtable/table.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

enum class DataType { kNotDefined, kPrice, kDividend };

constexpr int kMaxMutationBulkSize = 1000;

constexpr char kRowKeyDelimeter[] = "#";

// Create a namespace alias to make the code easier to read.
namespace cbt = ::google::cloud::bigtable;

// Assume input filepath is in the format of
// {ticker}_historical_{price|dividend}.csv, and parse out the ticker and
// data type.
bool ParseFilepath(const std::string& filepath, std::string* ticker,
                   DataType* data_type, std::string* col_family) {
  std::vector<absl::string_view> filepath_splitted =
      absl::StrSplit(filepath, '/', absl::SkipWhitespace());
  absl::string_view basename = filepath_splitted.back();

  std::vector<absl::string_view> basename_splitted =
      absl::StrSplit(basename, '.', absl::SkipWhitespace());
  if (basename_splitted.size() != 2) {
    std::cerr << "Invalid input basename: " << basename << std::endl;
    return false;
  }
  absl::string_view filename = basename_splitted.front();

  std::vector<absl::string_view> filename_splitted =
      absl::StrSplit(filename, '_', absl::SkipWhitespace());

  *ticker = absl::AsciiStrToUpper(filename_splitted.front());

  const std::string input_mode =
      absl::AsciiStrToUpper(filename_splitted.back());
  if (input_mode == "PRICE") {
    *data_type = DataType::kPrice;
    *col_family = "price";
  } else if (input_mode == "DIVIDEND") {
    *data_type = DataType::kDividend;
    *col_family = "dividend";
  } else {
    std::cerr << "Unrecognized input data type: " << filename_splitted.back()
              << std::endl;
    return false;
  }

  return true;
}

std::string PrepareRowKey(absl::string_view ticker, const absl::Time time) {
  auto year = time.In(absl::UTCTimeZone()).year;
  return absl::StrCat(ticker, kRowKeyDelimeter, year);
}

std::chrono::milliseconds PrepareTimestamp(const absl::Time time) {
  return absl::ToChronoMilliseconds(time - absl::UnixEpoch());
}

void CommitBulk(cbt::Table* table, cbt::BulkMutation* bulk_mutation) {
  std::cout << "Committing bulk mutation size " << bulk_mutation->size()
            << std::endl;
  std::vector<cbt::FailedMutation> failures =
      table->BulkApply(std::move(*bulk_mutation));

  if (!failures.empty()) {
    std::cerr << failures.size() << " of the mutations in the bulk failed."
              << std::endl;
  }

  // After the BulkApply() the bulk_mutation is discarded. Reinitialize.
  *bulk_mutation = cbt::BulkMutation();
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 5) {
    std::cerr << "Usage: populate_data <data_filepath> "
              << "<project_id> <instance_id> <table_id>" << std::endl;
    return 1;
  }

  // Prerequsite check.
  const std::string data_filepath = argv[1];
  const std::string project_id = argv[2];
  const std::string instance_id = argv[3];
  const std::string table_id = argv[4];
  if (data_filepath.empty() || project_id.empty() || instance_id.empty() ||
      table_id.empty()) {
    std::cerr << "Please specify necessary parameters." << std::endl;
    return 1;
  }

  DataType data_type = DataType::kNotDefined;
  std::string ticker, col_family;
  if (!ParseFilepath(data_filepath, &ticker, &data_type, &col_family)) {
    std::cerr << "Invalid input filepath: " << data_filepath << std::endl;
    return 1;
  }

  // Record the duration.
  absl::Time start_time = absl::Now();

  // Prepare the cloud bigtable.
  cbt::Table table(cbt::MakeDataClient(project_id, instance_id), table_id);
  std::cout << "table name" << table.table_name() << std::endl;
  cbt::BulkMutation bulk_mutation;
  int num_row_mutation = 0, num_cell_mutation = 0;

  // Read input data and populate the Bigtable.
  std::fstream input_file(data_filepath);
  if (!input_file.is_open()) {
    std::cerr << "Error in opening file: " << data_filepath << std::endl;
    return 1;
  }

  std::string line;
  while (std::getline(input_file, line)) {
    std::vector<std::string> line_splitted =
        absl::StrSplit(line, ',', absl::SkipWhitespace());
    auto splitted_ptr = line_splitted.begin();

    absl::Time time;
    std::string time_error;
    if (!absl::ParseTime("%Y-%m-%d", *splitted_ptr, &time, &time_error)) {
      // Skip the header.
      std::cout << "Can't parse the line: " << line << "; Continue."
                << std::endl;
      continue;
    }
    const std::string row_key = PrepareRowKey(ticker, time);
    const std::chrono::milliseconds timestamp = PrepareTimestamp(time);

    cbt::SingleRowMutation row_mutation(row_key);

    if (data_type == DataType::kPrice) {
      // Splitted line in the format of Date,Open,High,Low,Close,Adj
      // Close,Volume
      ++splitted_ptr;
      row_mutation.emplace_back(
          cbt::SetCell(col_family, "open_price", timestamp, *splitted_ptr));
      num_cell_mutation++;

      ++splitted_ptr;
      row_mutation.emplace_back(
          cbt::SetCell(col_family, "high_price", timestamp, *splitted_ptr));
      num_cell_mutation++;

      ++splitted_ptr;
      row_mutation.emplace_back(
          cbt::SetCell(col_family, "low_price", timestamp, *splitted_ptr));
      num_cell_mutation++;

      ++splitted_ptr;
      row_mutation.emplace_back(
          cbt::SetCell(col_family, "close_price", timestamp, *splitted_ptr));
      num_cell_mutation++;

      ++splitted_ptr;
      row_mutation.emplace_back(cbt::SetCell(col_family, "adj_close_price",
                                             timestamp, *splitted_ptr));
      num_cell_mutation++;

      // Don't use Volume.
    } else if (data_type == DataType::kDividend) {
      // Splitted line in the format of Date,Dividend
      ++splitted_ptr;
      row_mutation.emplace_back(
          cbt::SetCell(col_family, "dividend", timestamp, *splitted_ptr));
      num_cell_mutation++;
    }

    bulk_mutation.emplace_back(row_mutation);
    num_row_mutation++;
    if (bulk_mutation.size() == kMaxMutationBulkSize) {
      CommitBulk(&table, &bulk_mutation);
    }
  }

  // Flush the remaining mutations.
  CommitBulk(&table, &bulk_mutation);

  std::cout << "BigTable populated with data from file: " << data_filepath
            << std::endl
            << "Total num of row mutations: " << num_row_mutation << std::endl
            << "Total num of cell mutations: " << num_cell_mutation
            << std::endl;
  std::cout << "Total time used: " << absl::Now() - start_time << std::endl;

  return 0;
}
