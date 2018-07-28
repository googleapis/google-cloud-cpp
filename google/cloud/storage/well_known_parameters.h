// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_PARAMETERS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_PARAMETERS_H_

#include "google/cloud/internal/optional.h"
#include "google/cloud/storage/version.h"
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Refactor definition of well-known query parameters using the CRTP.
 *
 * @tparam P the type we will use to represent the query parameter.
 * @tparam T the C++ type of the query parameter
 */
template <typename P, typename T>
class WellKnownParameter {
 public:
  WellKnownParameter() : value_{} {}
  explicit WellKnownParameter(T&& value) : value_(std::forward<T>(value)) {}

  char const* parameter_name() const { return P::well_known_parameter_name(); }
  bool has_value() const { return value_.has_value(); }
  T const& value() const { return value_.value(); }

 private:
  google::cloud::internal::optional<T> value_;
};

template <typename P, typename T>
std::ostream& operator<<(std::ostream& os,
                         WellKnownParameter<P, T> const& rhs) {
  if (rhs.has_value()) {
    return os << rhs.parameter_name() << "=" << rhs.value();
  }
  return os << rhs.parameter_name() << "=<not set>";
}

struct Projection : public WellKnownParameter<Projection, std::string> {
  using WellKnownParameter<Projection, std::string>::WellKnownParameter;
  static char const* well_known_parameter_name() { return "projection"; }
};

struct UserProject : public WellKnownParameter<UserProject, std::string> {
  using WellKnownParameter<UserProject, std::string>::WellKnownParameter;
  static char const* well_known_parameter_name() { return "userProject"; }
};

struct IfGenerationMatch
    : public WellKnownParameter<IfGenerationMatch, std::int64_t> {
  using WellKnownParameter<IfGenerationMatch, std::int64_t>::WellKnownParameter;
  static char const* well_known_parameter_name() { return "ifGenerationMatch"; }
};

struct IfGenerationNotMatch
    : public WellKnownParameter<IfGenerationNotMatch, std::int64_t> {
  using WellKnownParameter<IfGenerationNotMatch,
                           std::int64_t>::WellKnownParameter;
  static char const* well_known_parameter_name() {
    return "ifGenerationNotMatch";
  }
};

struct IfMetaGenerationMatch
    : public WellKnownParameter<IfMetaGenerationMatch, std::int64_t> {
  using WellKnownParameter<IfMetaGenerationMatch,
                           std::int64_t>::WellKnownParameter;
  static char const* well_known_parameter_name() {
    return "ifMetagenerationMatch";
  }
};

struct IfMetaGenerationNotMatch
    : public WellKnownParameter<IfMetaGenerationNotMatch, std::int64_t> {
  using WellKnownParameter<IfMetaGenerationNotMatch,
                           std::int64_t>::WellKnownParameter;
  static char const* well_known_parameter_name() {
    return "ifMetagenerationNotMatch";
  }
};

struct Generation : public WellKnownParameter<Generation, std::int64_t> {
  using WellKnownParameter<Generation, std::int64_t>::WellKnownParameter;
  static char const* well_known_parameter_name() { return "generation"; }
};

struct Prefix : public WellKnownParameter<Prefix, std::string> {
  using WellKnownParameter<Prefix, std::string>::WellKnownParameter;
  static char const* well_known_parameter_name() { return "prefix"; }
};

struct MaxResults : public WellKnownParameter<MaxResults, std::int64_t> {
  using WellKnownParameter<MaxResults, std::int64_t>::WellKnownParameter;
  static char const* well_known_parameter_name() { return "maxResults"; }
};

struct UploadType : public WellKnownParameter<UploadType, std::string> {
  using WellKnownParameter<UploadType, std::string>::WellKnownParameter;
  static char const* well_known_parameter_name() { return "uploadType"; }
};

struct ContentEncoding
    : public WellKnownParameter<ContentEncoding, std::string> {
  using WellKnownParameter<ContentEncoding, std::string>::WellKnownParameter;
  static char const* well_known_parameter_name() { return "contentEncoding"; }
};

struct KmsKeyName : public WellKnownParameter<KmsKeyName, std::string> {
  using WellKnownParameter<KmsKeyName, std::string>::WellKnownParameter;
  static char const* well_known_parameter_name() { return "kmsKeyName"; }
};

struct PredefinedAcl : public WellKnownParameter<PredefinedAcl, std::string> {
  using WellKnownParameter<PredefinedAcl, std::string>::WellKnownParameter;
  static char const* well_known_parameter_name() { return "predefinedAcl"; }
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_PARAMETERS_H_
