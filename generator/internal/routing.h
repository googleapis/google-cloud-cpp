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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_ROUTING_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_ROUTING_H

#include "google/protobuf/descriptor.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {

/// Our representation for a `google.api.RoutingParameter` message.
struct RoutingParameter {
  /**
   * A processed `field` string from a `RoutingParameter` proto.
   *
   * It has potentially been modified to avoid naming conflicts (e.g. a proto
   * field named "namespace" will be stored here as "namespace_").
   *
   * We will generate code like: `"request." + field_name + "();"` in our
   * metadata decorator to access the field's value.
   */
  std::string field_name;

  /**
   * A processed `path_template` string from a `RoutingParameter` proto.
   *
   * It is translated for use as a `std::regex`. We make the following
   * substitutions, simultaneously:
   *   - "**"    => ".*"
   *   - "*"     => "[^/]+"
   *   - "{foo=" => "("
   *   - "}"     => ")"
   *
   * Note that we do not store the routing parameter key ("foo" in this example)
   * in this struct. It is instead stored as a key in the `ExplicitRoutingInfo`
   * map.
   */
  std::string pattern;
};

/**
 * A data structure to represent the logic of a `google.api.RoutingRule`, in a
 * form that facilitates code generation.
 *
 * The keys of the map are the extracted routing param keys. They map to an
 * ordered list of matching rules. For this object, the first match will win.
 */
using ExplicitRoutingInfo =
    std::unordered_map<std::string, std::vector<RoutingParameter>>;

/**
 * Parses the explicit resource routing info as defined in the
 * google.api.routing annotation.
 *
 * This method processes the `google.api.RoutingRule` proto. It groups the
 * `google.api.RoutingParameters` by the extracted routing parameter key.
 *
 * Each `google.api.RoutingParameter` message is translated into a form that is
 * easier for our generator to work with (see `RoutingParameter` above).
 *
 * We reverse the order of the `RoutingParameter`s. The rule (as defined in
 * go/actools-dynamic-routing-proposal) is that "last wins". We would like to
 * order them such that "first wins", so we can stop iterating when we have
 * found a match.
 */
ExplicitRoutingInfo ParseExplicitRoutingHeader(
    google::protobuf::MethodDescriptor const& method);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_ROUTING_H
