// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/internal/discovery_doc.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

ABSL_FLAG(bool, preferred, true,
          "Is this rest discovery entry marked as preferred.");
ABSL_FLAG(std::string, api_index_json_path, "",
          "Path to googleapis/api-index-v1.json file.");
ABSL_FLAG(std::string, gcp_services_path, "",
          "Path to gcp_services.json file.");
ABSL_FLAG(std::string, output_dir_path, "./output",
          "Path to root dir for emitted protos.");

int main(int argc, char** argv) try {
  absl::ParseCommandLine(argc, argv);
  google::cloud::generator_internal::DoDiscovery();
  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
