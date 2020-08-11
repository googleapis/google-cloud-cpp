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
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_GENERATOR_H
#define GOOGLE_CLOUD_CPP_GENERATOR_GENERATOR_H

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <string>

namespace google {
namespace cloud {
namespace generator {

/**
 * C++ microgenerator plugin entry point.
 *
 * Command line arguments can be passed from the protoc command line via:
 * '--cpp_codegen_opt=key=value'. This can be specified multiple times to
 * pass various key,value pairs.
 *
 * Generated files will be written to a path determined by concatenating the
 * paths in --cpp_codegen_out=path and --cpp_codegen_opt=product_path=path.
 *
 * @par Example:
 * @code
 * protoc \
 *   --proto_path=${MY_PROTO_PATH} \
 *   --plugin=protoc-gen-cpp_codegen=${PLUGIN_BIN_PATH}/protoc-gen-cpp_codegen \
 *   --cpp_codegen_out=. \
 *   --cpp_codegen_opt=product_path=google/cloud/spanner
 * @endcode
 */
class Generator : public google::protobuf::compiler::CodeGenerator {
 public:
  bool Generate(google::protobuf::FileDescriptor const* file,
                std::string const& parameter,
                google::protobuf::compiler::GeneratorContext* generator_context,
                std::string* error) const override;
};

}  // namespace generator
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_GENERATOR_H
