// Copyright 2020 Google LLC
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
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_PRINTER_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_PRINTER_H

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Wrapper around a google::protobuf::io::ZeroCopyOutputStream and a
 * google::protobuf::io::Printer object so that they can be used for
 * code generation.
 */
class Printer {
 public:
  Printer(google::protobuf::compiler::GeneratorContext* generator_context,
          std::string const& file_name)
      : output_(generator_context->Open(file_name)),
        printer_(absl::make_unique<google::protobuf::io::Printer>(
            output_.get(), '$', nullptr)) {}

  /**
   * Print some text after applying variable substitutions.
   *
   * If a particular variable in the text is not defined, this will crash.
   * Variables to be substituted are identified by their names surrounded by
   * delimiter characters (as given to the constructor).  The variable bindings
   * are defined by the given map.
   */
  void Print(const std::map<std::string, std::string>& variables,
             absl::string_view text) {
    printer_->Print(variables, text.data());
  }

  /**
   * Like the first Print(), except the substitutions are given as parameters.
   */
  template <typename... Args>
  void Print(absl::string_view text, const Args&... args) {
    printer_->Print(text.data(), args...);
  }

  Printer(Printer const&) = delete;
  Printer& operator=(Printer const&) = delete;
  Printer(Printer&&) = default;
  Printer& operator=(Printer&&) = default;

 private:
  std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> output_;
  std::unique_ptr<google::protobuf::io::Printer> printer_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_PRINTER_H
