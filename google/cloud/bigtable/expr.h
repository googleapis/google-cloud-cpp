// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXPR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXPR_H

#include "google/cloud/bigtable/version.h"
#include <google/type/expr.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Create a google::type::Expr.
 *
 * This is a textual representation of an expression in Common Expression
 * Language (CEL) syntax.
 *
 * @param expression the expression in Common Expression Language.
 * @param title an optional title for the expression, i.e. a short string
 *     describing its purpose.
 * @param description an optional description of the expression. This is a
 *     longer text which describes the expression, e.g. when hovered over it in
 *     a UI.
 * @param location an optional string indicating the location of the expression
 *     for error reporting, e.g. a file name and a position in the file.
 */
google::type::Expr Expression(std::string expression, std::string title = "",
                              std::string description = "",
                              std::string location = "");

std::ostream& operator<<(std::ostream& stream, google::type::Expr const&);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXPR_H
