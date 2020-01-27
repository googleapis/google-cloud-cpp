// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "gcs_indexer_constants.h"
#include <google/cloud/spanner/client.h>
#include <boost/program_options.hpp>
#include <algorithm>
#include <iostream>
#include <vector>

namespace po = boost::program_options;
namespace spanner = google::cloud::spanner;

int main(int argc, char* argv[]) try {
  auto get_env = [](std::string_view name) -> std::string {
    auto value = std::getenv(name.data());
    return value == nullptr ? std::string{} : value;
  };

  po::positional_options_description positional;
  positional.add("bucket", -1);
  po::options_description options("Create a GCS indexing database");
  options.add_options()("help", "produce help message")
      //
      ("bucket", po::value<std::vector<std::string>>()->required(),
       "the bucket to un-index")
      //
      ("project",
       po::value<std::string>()->default_value(get_env("GOOGLE_CLOUD_PROJECT")),
       "set the Google Cloud Platform project id")
      //
      ("instance", po::value<std::string>()->required(),
       "set the Cloud Spanner instance id")
      //
      ("database", po::value<std::string>()->required(),
       "set the Cloud Spanner database id");

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
                .options(options)
                .positional(positional)
                .run(),
            vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << options << "\n";
    return 0;
  }
  for (auto arg : {"project", "instance", "database"}) {
    if (vm.count(arg) != 1 || vm[arg].as<std::string>().empty()) {
      std::cout << "The --" << arg
                << " option must be set to a non-empty value\n"
                << options << "\n";
      return 1;
    }
  }
  if (vm.count("bucket") == 0) {
    std::cout << "You must specific at least one bucket to deindex\n"
              << options << "\n";
    return 1;
  }

  spanner::Database const database(vm["project"].as<std::string>(),
                                   vm["instance"].as<std::string>(),
                                   vm["database"].as<std::string>());

  auto spanner_client =
      spanner::Client(spanner::MakeConnection(std::move(database)));

  for (auto const& bucket : vm["bucket"].as<std::vector<std::string>>()) {
    std::cout << "Deleting data for bucket=<" << bucket << ">" << std::endl;
    auto result = spanner_client.ExecutePartitionedDml(spanner::SqlStatement(
        "DELETE FROM " + std::string(table_name) + " WHERE bucket = @bucket",
        {{"bucket", spanner::Value(bucket)}}));
    if (not result) {
      std::cerr << "Error deleting index for bucket=<" << bucket
                << ">: " << result.status() << "\n";
    }
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception caught " << ex.what() << '\n';
  return 1;
}
