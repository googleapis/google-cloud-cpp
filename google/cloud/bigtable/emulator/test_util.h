#include "google/cloud/bigtable/emulator/table.h"
#include <memory>
#include <string>

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

Status SetCells(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& table_name, std::string const& row_key,
    std::vector<SetCellParams>& set_cell_params);

StatusOr<std::shared_ptr<Table>> CreateTable(
    std::string const& table_name, std::vector<std::string>& column_families);

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
