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

#include "google/cloud/bigtable/expr.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
google::type::Expr Expression(std::string expression, std::string title,
                              std::string description, std::string location) {
  google::type::Expr expr;
  expr.set_expression(std::move(expression));
  expr.set_title(std::move(title));
  expr.set_description(std::move(description));
  expr.set_location(std::move(location));
  return expr;
}

std::ostream& operator<<(std::ostream& stream, google::type::Expr const& e) {
  stream << "(" << e.expression();
  if (!e.title().empty()) {
    stream << ", title=\"" << e.title() << "\"";
  }
  if (!e.description().empty()) {
    stream << ", description=\"" << e.description() << "\"";
  }
  if (!e.location().empty()) {
    stream << ", location=\"" << e.location() << "\"";
  }
  stream << ")";
  return stream;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
