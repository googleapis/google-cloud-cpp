// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc/metrics_exporter_options.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/options.h"
#include "google/cloud/project.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/universe_domain_options.h"
#include <gmock/gmock.h>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(MetricsExporterOptions, DefaultEndpoint) {
  // In all these cases there default monitoring endpoint is either the best
  // choice, or the least bad choice.
  struct TestCase {
    std::string storage_endpoint;
    std::string universe_domain;
  } const cases[] = {
      {"", ""},
      {"storage.googleapis.com", ""},
      {"storage.ud.net", ""},
      {"storage-foo.p.googleapis.com", ""},
      {"", "ud.net"},
      {"storage.ud.net", "ud.net"},
      {"storage.googleapis.com", "ud.net"},
      {"private.googleapis.com", "ud.net"},
      {"restricted.googleapis.com", "ud.net"},
      {"storage-foo.p.googleapis.com", "ud.net"},
  };

  for (auto const& t : cases) {
    SCOPED_TRACE("Testing with " + t.storage_endpoint + " and " +
                 t.universe_domain);
    auto options = Options{};
    if (!t.storage_endpoint.empty()) {
      options.set<EndpointOption>(t.storage_endpoint);
    }
    auto expected_ud = std::string{};
    if (!t.universe_domain.empty()) {
      options.set<internal::UniverseDomainOption>(t.universe_domain);
      expected_ud = t.universe_domain;
    }
    options = DefaultOptionsGrpc(std::move(options));
    auto const actual = MetricsExporterOptions(options);
    EXPECT_FALSE(actual.has<EndpointOption>());
    EXPECT_EQ(actual.get<internal::UniverseDomainOption>(), expected_ud);
  }
}

TEST(MetricsExporterOptions, PrivateDefaultUD) {
  auto actual = MetricsExporterOptions(
      Options{}.set<EndpointOption>("private.googleapis.com"));
  EXPECT_THAT(actual.get<EndpointOption>(), "private.googleapis.com");
}

TEST(MetricsExporterOptions, PrivateUD) {
  auto actual = MetricsExporterOptions(
      Options{}
          .set<EndpointOption>("private.ud.net")
          .set<internal::UniverseDomainOption>("ud.net"));
  EXPECT_THAT(actual.get<EndpointOption>(), "private.ud.net");
}

TEST(MetricsExporterOptions, RestrictedDefaultUD) {
  auto actual = MetricsExporterOptions(
      Options{}.set<EndpointOption>("restricted.googleapis.com"));
  EXPECT_THAT(actual.get<EndpointOption>(), "restricted.googleapis.com");
}

TEST(MetricsExporterOptions, RestrictedUD) {
  auto actual = MetricsExporterOptions(
      Options{}
          .set<EndpointOption>("restricted.ud.net")
          .set<internal::UniverseDomainOption>("ud.net"));
  EXPECT_THAT(actual.get<EndpointOption>(), "restricted.ud.net");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
