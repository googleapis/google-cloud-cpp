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
#ifndef GENERATOR_INTERNAL_PRINTER_H_
#define GENERATOR_INTERNAL_PRINTER_H_

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <string>

namespace google {
namespace cloud {
namespace generator {
namespace internal {

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
        printer_(output_.get(), '$', NULL) {}

  google::protobuf::io::Printer* operator->() & { return &printer_; }
  google::protobuf::io::Printer const* operator->() const& { return &printer_; }

  Printer(Printer const&) = delete;
  Printer& operator=(Printer const&) = delete;

 private:
  std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> output_;
  google::protobuf::io::Printer printer_;
};

}  // namespace internal
}  // namespace generator
}  // namespace cloud
}  // namespace google

#endif  // GENERATOR_INTERNAL_PRINTER_H_
