// Copyright 2018 Google LLC
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

//! [all code]

//! [bigtable includes]
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
//! [bigtable includes]

#include "opencensus/exporters/stats/stackdriver/stackdriver_exporter.h"
#include "opencensus/exporters/stats/stdout/stdout_exporter.h"
#include "opencensus/exporters/trace/stackdriver/stackdriver_exporter.h"
#include "opencensus/exporters/trace/stdout/stdout_exporter.h"
#include "opencensus/stats/stats.h"
#include "opencensus/trace/sampler.h"
#include "opencensus/trace/trace_config.h"
#include <grpcpp/opencensus.h>

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project_id> <instance_id> <table_id>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table_id = argv[3];

  // Register the OpenCensus gRPC plugin to enable stats and tracing in gRPC.
  grpc::RegisterOpenCensusPlugin();

  // The `ProbabilitySampler` used in this example samples out request and
  // does not trace all the requests. So, if production system requires tracing
  // of each request then different samplers must be used
  //
  // For more details, see the documentation
  //
  //   https://opencensus.io/core-concepts/tracing/#sampling
  //   https://github.com/census-instrumentation/opencensus-specs/blob/master/trace/Sampling.md#sampling
  //

  opencensus::trace::TraceConfig::SetCurrentTraceParams(
      {128, 128, 128, 128, opencensus::trace::ProbabilitySampler(1.0)});

  // For debugging, register exporters that just write to stdout.
  opencensus::exporters::stats::StdoutExporter::Register();
  opencensus::exporters::trace::StdoutExporter::Register();

  // Registration of Stackdriver requires couple of parameters,
  // project_id The Stackdriver Project ID to use
  // opencensus_task The opencensus_task is used to uniquely identify the task
  //   in Stackdriver. The recommended format is "{LANGUAGE}-{PID}@{HOSTNAME}".
  //   If PID is not available, a random number may be used.
  //
  // For more details, see the documentation
  //   https://github.com/census-instrumentation/opencensus-cpp/tree/master/opencensus/exporters/stats/stackdriver#opencensus-stackdriver-stats-exporter

  opencensus::exporters::stats::StackdriverExporter::Register(
      project_id, "bigtable-opencensus-0@unspecified-host");
  opencensus::exporters::trace::StackdriverExporter::Register(project_id);

  // Connect to the Cloud Bigtable Admin API.
  //! [connect admin]
  google::cloud::bigtable::TableAdmin table_admin(
      google::cloud::bigtable::CreateDefaultAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()),
      instance_id);
  //! [connect admin]

  //! [create table]
  // Define the desired schema for the Table.
  auto gc_rule = google::cloud::bigtable::GcRule::MaxNumVersions(1);
  google::cloud::bigtable::TableConfig schema({{"family", gc_rule}}, {});

  // Create a table.
  auto returned_schema = table_admin.CreateTable(table_id, schema);
  //! [create table]

  // Create an object to access the Cloud Bigtable Data API.
  //! [connect data]
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id);
  //! [connect data]

  // Modify (and create if necessary) a row.
  //! [write rows]
  std::vector<std::string> greetings{"Hello World!", "Hello Cloud Bigtable!",
                                     "Hello C++!"};
  int i = 0;
  for (auto const& greeting : greetings) {
    // Each row has a unique row key.
    //
    // Note: This example uses sequential numeric IDs for simplicity, but
    // this can result in poor performance in a production application.
    // Since rows are stored in sorted order by key, sequential keys can
    // result in poor distribution of operations across nodes.
    //
    // For more information about how to design a Bigtable schema for the
    // best performance, see the documentation:
    //
    //     https://cloud.google.com/bigtable/docs/schema-design
    std::string row_key = "key-" + std::to_string(i);
    table.Apply(google::cloud::bigtable::SingleRowMutation(
        std::move(row_key),
        google::cloud::bigtable::SetCell("family", "c0", greeting)));
    ++i;
  }
  //! [write rows]

  // Read a single row.
  //! [read row]
  auto result = table.ReadRow(
      "key-0",
      google::cloud::bigtable::Filter::ColumnRangeClosed("family", "c0", "c0"));
  if (!result.first) {
    std::cout << "Cannot find row 'key-0' in the table: " << table.table_name()
              << std::endl;
    return 0;
  }
  auto const& cell = result.second.cells().front();
  std::cout << cell.family_name() << ":" << cell.column_qualifier() << "    @ "
            << cell.timestamp().count() << "us\n"
            << '"' << cell.value() << '"' << std::endl;
  //! [read row]

  // Read a single row.
  //! [scan all]
  for (auto& row :
       table.ReadRows(google::cloud::bigtable::RowRange::InfiniteRange(),
                      google::cloud::bigtable::Filter::PassAllFilter())) {
    std::cout << row.row_key() << ":\n";
    for (auto& cell : row.cells()) {
      std::cout << "\t" << cell.family_name() << ":" << cell.column_qualifier()
                << "    @ " << cell.timestamp().count() << "us\n"
                << "\t\"" << cell.value() << '"' << std::endl;
    }
  }
  //! [scan all]

  // Delete the table
  //! [delete table]
  table_admin.DeleteTable(table_id);
  //! [delete table]

  // Stop tracing because the remaining RPCs are OpenCensus related.
  opencensus::trace::TraceConfig::SetCurrentTraceParams(
      {128, 128, 128, 128, opencensus::trace::ProbabilitySampler(0.0)});

  std::cout << "Sleeping to give exporters time " << std::flush;
  for (int i = 0; i != 30; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << '.' << std::flush;
  }
  std::cout << " DONE" << std::endl;

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
//! [all code]
