// copyright 2019 google llc
//
// licensed under the apache license, version 2.0 (the "license");
// you may not use this file except in compliance with the license.
// you may obtain a copy of the license at
//
//     http://www.apache.org/licenses/license-2.0
//
// unless required by applicable law or agreed to in writing, software
// distributed under the license is distributed on an "as is" basis,
// without warranties or conditions of any kind, either express or implied.
// see the license for the specific language governing permissions and
// limitations under the license.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXPR_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXPR_H_

#include "google/type/expr.pb.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace GOOGLE_CLOUD_CPP_NS {
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

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXPR_H_
