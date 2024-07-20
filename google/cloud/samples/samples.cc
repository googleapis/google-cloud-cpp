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

#include "google/cloud/project.h"
#include "google/cloud/testing_util/example_driver.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using ::google::cloud::testing_util::Usage;

void StatusOrUsage(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv[0] == "--help") {
    throw Usage{"status-or-usage <project-name>"};
  }
  //! [status-or-usage]
  namespace gc = ::google::cloud;
  [](std::string const& project_name) {
    gc::StatusOr<gc::Project> project = gc::MakeProject(project_name);
    if (!project) {
      std::cerr << "Error parsing project <" << project_name
                << ">: " << project.status() << "\n";
      return;
    }
    std::cout << "The project id is " << project->project_id() << "\n";
  }
  //! [status-or-usage]
  (argv.at(0));
}

void StatusOrExceptions(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv[0] == "--help") {
    throw Usage{"status-or-exceptions <project-name>"};
  }
  //! [status-or-exceptions]
  namespace gc = ::google::cloud;
  [](std::string const& project_name) {
    try {
      gc::Project project = gc::MakeProject(project_name).value();
      std::cout << "The project id is " << project.project_id() << "\n";
    } catch (gc::RuntimeStatusError const& ex) {
      std::cerr << "Error parsing project <" << project_name
                << ">: " << ex.status() << "\n";
    }
  }
  //! [status-or-exceptions]
  (argv.at(0));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (!argv.empty()) throw examples::Usage{"auto"};

  std::cout << "\nRunning StatusOrUsage() example [1]" << std::endl;
  StatusOrUsage({"invalid-project-name"});

  std::cout << "\nRunning StatusOrUsage() example [2]" << std::endl;
  StatusOrUsage({"projects/my-project-id"});

  std::cout << "\nRunning StatusOrExceptions() example [1]" << std::endl;
  StatusOrExceptions({"invalid-project-name"});

  std::cout << "\nRunning StatusOrExceptions() example [2]" << std::endl;
  StatusOrExceptions({"projects/my-project-id"});
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"status-or-usage", StatusOrUsage},
      {"status-or-exceptions", StatusOrExceptions},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
