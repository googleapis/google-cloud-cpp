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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_FORMAT_METHOD_COMMENTS_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_FORMAT_METHOD_COMMENTS_H

#include <google/protobuf/descriptor.h>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Formats comments from the source .proto file into Doxygen compatible
 * function headers, including param and return lines as necessary.
 */
std::string FormatMethodComments(
    google::protobuf::MethodDescriptor const& method,
    std::string const& variable_parameter_comments,
    bool is_discovery_document_proto);

/**
 * If there were any method comment substitutions that went unused, log
 * errors about them and return false. Otherwise do nothing and return true.
 */
bool CheckMethodCommentSubstitutions();

/**
 * Comments for LRO Start overload.
 */
std::string FormatStartMethodComments(bool is_method_deprecated);

/**
 * Comments for LRO Await overload.
 */
std::string FormatAwaitMethodComments(bool is_method_deprecated);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_FORMAT_METHOD_COMMENTS_H
