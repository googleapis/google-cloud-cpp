// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/testing_util/example_driver.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/status.h"
#include <iostream>

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

Example::Example(std::map<std::string, CommandType> commands)
    : commands_(std::move(commands)) {
  // Force each command to generate its Usage string, so we can provide a good
  // usage string for the whole program.
  for (auto const& kv : commands_) {
    if (kv.first == "auto") continue;
    try {
      kv.second({"--help"});
    } catch (Usage const& u) {
      full_usage_ += "    ";
      full_usage_ += u.what();
      full_usage_ += "\n";
    }
  }
}

int Example::Run(int argc, char const* const argv[]) try {

  std::vector<std::string> commandLineArgs(argc-1);
  for(int i=0;i<argc-1;i++) {
    commandLineArgs[i] = argv[i+1];
  }
  bool auto_run =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
          .value_or("") == "yes";
  if (argc == 1 && auto_run) {
    auto entry = commands_.find("auto");
    if (entry == commands_.end()) {
      PrintUsage(argv[0], "Requested auto run but there is no 'auto' command",1);
      return 1;
    }
    entry->second({});
    return 0;
  }

  if (argc < 2) {
    PrintUsage(argv[0], "Missing command",1);
    return 1;
  }

std::string::size_type flagIndex = 0;
std::string::size_type cmdIndex = 0;
for(std::string::size_type i=0;i<commandLineArgs.size();i++) {
  if(commandLineArgs[i] == "--help"){
      flagIndex = i+1;
      break;
  }
}
for(std::string::size_type i=0;i<commandLineArgs.size();i++) {
  if(commands_.find(commandLineArgs[i]) != commands_.end()) {
    cmdIndex = i+1;
    break;
  }  
}

if(flagIndex == (size_t)0 and cmdIndex != (size_t)0) {
  PrintUsage(commandLineArgs[cmdIndex-1], "",0);
  return 1;
}

else if(flagIndex != (size_t)0 and cmdIndex != (size_t)0){
  PrintUsage(commandLineArgs[cmdIndex-1],"",0);
  return 0;
}

else if(flagIndex == (size_t)0 and cmdIndex == (size_t)0) {
  PrintUsage(argv[0], "",1);
  return 1;
}

else {
  PrintUsage(argv[0], "",1);
  return 0;
}

} catch (Usage const& u) {
  PrintUsage(argv[0], u.what(),1);
  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  google::cloud::LogSink::Instance().Flush();
  throw;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  google::cloud::LogSink::Instance().Flush();
  throw;
}

void Example::PrintUsage(std::string const& cmd, std::string const& msg, int const& printFullUsage) {
  auto last_slash = cmd.find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  if (!msg.empty()) std::cerr << msg << "\n";
  if(printFullUsage){
  std::cerr << "Usage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n" << full_usage_;
  }
  else {
    std::cerr << "Usage: " << program << " <command> [arguments]\n\n";
  }

}

void CheckEnvironmentVariablesAreSet(std::vector<std::string> const& vars) {
  for (auto const& var : vars) {
    auto const value = google::cloud::internal::GetEnv(var.c_str());
    if (!value) {
      throw std::runtime_error("The " + var +
                               " environment variable is not set");
    }
    if (value->empty()) {
      throw std::runtime_error("The " + var +
                               " environment variable has an empty value");
    }
  }
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
