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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_EXAMPLE_DRIVER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_EXAMPLE_DRIVER_H

#include "google/cloud/internal/port_platform.h"
#include "google/cloud/version.h"
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// The examples only compile when exceptions are enabled, we prefer clarify over
// portability for them.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

/// Report errors parsing the command-line.
class Usage : public std::runtime_error {
 public:
  explicit Usage(std::string const& msg) : std::runtime_error(msg) {}
};

/// A short code example callable from the command-line
using CommandType = std::function<void(std::vector<std::string> const& argv)>;

/// Code same names and the functions that implement them.
using Commands = std::map<std::string, CommandType>;

/**
 * Drive the execution of code examples for the Cloud C++ client libraries.
 *
 * @note the examples always assume that exceptions are enabled. We prefer
 * clarity over portability for example code.
 *
 * We often (ideally always) write examples showing how to use each key API in
 * the client libraries. These examples are executed as part of the CI builds,
 * but we also want to offer a simple command-line interface to run the
 * example. Our documentation may say something like:
 *
 * @code
 * To run the ReadRows example use:
 *
 * bazel run //google/cloud/spanner/samples:samples -- read-rows <blah blah>
 * @endcode
 *
 * We found ourselves writing the same driver code over and over to both (a) run
 * one specific example chosen from the command-line, and (b) run all the
 * examples in a specific sequence for the CI builds.
 *
 * This class refactors this common code. In general, we write the examples as
 * (anmed) short functions, which may receive arguments (such as project ids)
 * from the command line (as a `std::vector<std::string>`).
 *
 * The `auto` function name is special, it is invoked automatically if no
 * arguments are provided to the example program *and* the
 * `GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES` environment variable is set to `yes`.
 */
class Example {
 public:
  explicit Example(std::map<std::string, CommandType> commands);

  int Run(int argc, char const* const argv[]);

 private:
  void PrintUsage(std::string const& cmd, std::string const& msg);

  std::map<std::string, CommandType> commands_;
  std::string full_usage_;
};

/// Verify that a list of environment variables are set or throw.
void CheckEnvironmentVariablesAreSet(std::vector<std::string> const&);

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_EXAMPLE_DRIVER_H
