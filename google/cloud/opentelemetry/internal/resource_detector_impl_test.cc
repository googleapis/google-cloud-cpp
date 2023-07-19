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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/opentelemetry/internal/resource_detector_impl.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/compute_engine_util.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace sc = opentelemetry::sdk::resource::SemanticConventions;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::OTelAttribute;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ms = std::chrono::milliseconds;

auto expect_query = [] {
  auto const expected_path =
      absl::StrCat(internal::GceMetadataScheme(), "://",
                   internal::GceMetadataHostname(), "/computeMetadata/v1/");
  return AllOf(Property(&rest_internal::RestRequest::path, expected_path),
               Property(&rest_internal::RestRequest::headers,
                        Contains(Pair("metadata-flavor", Contains("Google")))),
               Property(&rest_internal::RestRequest::parameters,
                        Contains(Pair("recursive", "true"))));
};

std::multimap<std::string, std::string> ExpectedHeaders() {
  return {{"content-type", "application/json; charset=utf-8"},
          {"metadata-flavor", "google"}};
}

std::unique_ptr<opentelemetry::sdk::resource::ResourceDetector>
MakeTestDetector(otel_internal::HttpClientFactory factory) {
  return otel_internal::MakeResourceDetector(
      std::move(factory),
      internal::LimitedErrorCountRetryPolicy<otel_internal::StatusTraits>(0)
          .clone(),
      ExponentialBackoffPolicy(ms(0), ms(0), 2.0).clone(), Options{});
}

std::unique_ptr<opentelemetry::sdk::resource::ResourceDetector>
MakeTestDetector(char const* payload) {
  auto factory = [=](Options const&) {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
    EXPECT_CALL(*response, Headers).WillOnce(Return(ExpectedHeaders()));
    EXPECT_CALL(std::move(*response), ExtractPayload)
        .WillOnce(
            Return(ByMove(MakeMockHttpPayloadSuccess(std::string{payload}))));

    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(_, expect_query()))
        .WillOnce(Return(ByMove(std::unique_ptr<rest_internal::RestResponse>(
            std::move(response)))));
    return mock;
  };
  return MakeTestDetector(std::move(factory));
}

TEST(ResourceDetector, RetriesTransientConnectionError) {
  testing_util::ScopedLog log;
  auto constexpr kNumRetries = 3;

  auto factory = [=](Options const&) {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(_, expect_query()))
        .Times(kNumRetries + 1)
        .WillRepeatedly(
            []() -> StatusOr<std::unique_ptr<rest_internal::RestResponse>> {
              return internal::UnavailableError("try again");
            });
    return mock;
  };

  auto detector = otel_internal::MakeResourceDetector(
      std::move(factory),
      internal::LimitedErrorCountRetryPolicy<otel_internal::StatusTraits>(
          kNumRetries)
          .clone(),
      ExponentialBackoffPolicy(ms(0), ms(0), 2.0).clone(), Options{});
  auto resource = detector->Detect();
  auto const& attributes = resource.GetAttributes();

  EXPECT_THAT(
      attributes,
      Not(Contains(OTelAttribute<std::string>(sc::kCloudProvider, "gcp"))));

  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(HasSubstr("Could not query the metadata server"),
                     HasSubstr("UNAVAILABLE"), HasSubstr("try again"))));
}

TEST(ResourceDetector, PermanentConnectionError) {
  testing_util::ScopedLog log;

  auto factory = [](Options const&) {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(_, expect_query()))
        .WillOnce(Return(internal::AbortedError("fail")));
    return mock;
  };

  auto detector = MakeTestDetector(factory);
  auto resource = detector->Detect();
  auto const& attributes = resource.GetAttributes();

  EXPECT_THAT(
      attributes,
      Not(Contains(OTelAttribute<std::string>(sc::kCloudProvider, "gcp"))));

  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("Could not query the metadata server"),
                             HasSubstr("ABORTED"), HasSubstr("fail"))));
}

TEST(ResourceDetector, HttpError) {
  testing_util::ScopedLog log;

  auto factory = [](Options const&) {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(rest_internal::HttpStatusCode::kForbidden));
    EXPECT_CALL(std::move(*response), ExtractPayload)
        .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(std::string{}))));

    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(_, expect_query()))
        .WillOnce(Return(ByMove(std::unique_ptr<rest_internal::RestResponse>(
            std::move(response)))));
    return mock;
  };

  auto detector = MakeTestDetector(factory);
  auto resource = detector->Detect();
  auto const& attributes = resource.GetAttributes();

  EXPECT_THAT(
      attributes,
      Not(Contains(OTelAttribute<std::string>(sc::kCloudProvider, "gcp"))));

  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("Could not query the metadata server"),
                             HasSubstr("PERMISSION_DENIED"))));
}

TEST(ResourceDetector, ValidatesHeaders) {
  for (auto const& bad_headers :
       std::vector<std::multimap<std::string, std::string>>{
           {},
           {{"content-type", "application/json"}},
           {{"metadata-flavor", "google"}},
           {{"content-type", "wrong"}, {"metadata-flavor", "google"}},
           {{"content-type", "application/json"}, {"metadata-flavor", "wrong"}},
       }) {
    testing_util::ScopedLog log;

    auto factory = [&bad_headers](Options const&) {
      auto response = std::make_unique<MockRestResponse>();
      EXPECT_CALL(*response, StatusCode)
          .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
      EXPECT_CALL(*response, Headers).WillRepeatedly(Return(bad_headers));

      auto mock = std::make_unique<MockRestClient>();
      EXPECT_CALL(*mock, Get(_, expect_query()))
          .WillOnce(Return(ByMove(std::unique_ptr<rest_internal::RestResponse>(
              std::move(response)))));
      return mock;
    };

    auto detector = MakeTestDetector(factory);
    auto resource = detector->Detect();
    auto const& attributes = resource.GetAttributes();

    EXPECT_THAT(
        attributes,
        Not(Contains(OTelAttribute<std::string>(sc::kCloudProvider, "gcp"))));

    EXPECT_THAT(
        log.ExtractLines(),
        Contains(AllOf(HasSubstr("Could not query the metadata server"),
                       HasSubstr("NOT_FOUND"), HasSubstr("response headers"))));
  }
}

TEST(ResourceDetector, PayloadReadError) {
  testing_util::ScopedLog log;

  auto factory = [](Options const&) {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
    EXPECT_CALL(*response, Headers).WillOnce(Return(ExpectedHeaders()));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([] {
      auto payload = std::make_unique<MockHttpPayload>();
      EXPECT_CALL(*payload, Read)
          .WillOnce(Return(internal::AbortedError("fail")));
      return payload;
    });

    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(_, expect_query()))
        .WillOnce(Return(ByMove(std::unique_ptr<rest_internal::RestResponse>(
            std::move(response)))));
    return mock;
  };

  auto detector = MakeTestDetector(factory);
  auto resource = detector->Detect();
  auto const& attributes = resource.GetAttributes();

  EXPECT_THAT(
      attributes,
      Not(Contains(OTelAttribute<std::string>(sc::kCloudProvider, "gcp"))));

  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("Could not query the metadata server"),
                             HasSubstr("ABORTED"), HasSubstr("fail"))));
}

TEST(ResourceDetector, HandlesBadJson) {
  auto constexpr kMissingKeysJson = R"json({})json";
  auto constexpr kMalformedJson = R"json({{})json";
  auto constexpr kWrongTypeJson = R"json({
  "instance": [],
  "project": {
    "projectId": "test-project"
  }
})json";
  auto constexpr kWrongStructureJson = R"json({
  "instance": {
    "machineType": {
      "unexpected": 5
    }
  },
  "project": {
    "projectId": "test-project"
  }
})json";

  for (auto const* payload : {kMissingKeysJson, kMalformedJson, kWrongTypeJson,
                              kWrongStructureJson}) {
    auto detector = MakeTestDetector(payload);
    (void)detector->Detect();
  }
}

TEST(ResourceDetector, GkeRegion) {
  testing_util::ScopedEnvironment env("KUBERNETES_SERVICE_HOST", "0.0.0.0");
  auto constexpr kPayload = R"json({
  "instance": {
    "attributes": {
      "cluster-name": "test-cluster",
      "cluster-location": "projects/1234567890/regions/us-central1"
    },
    "id": 1020304050607080900
  },
  "project": {
    "projectId": "test-project"
  }
})json";

  auto detector = MakeTestDetector(kPayload);
  auto resource = detector->Detect();
  auto const& attributes = resource.GetAttributes();

  EXPECT_THAT(
      attributes,
      IsSupersetOf({
          OTelAttribute<std::string>(sc::kCloudProvider, "gcp"),
          OTelAttribute<std::string>(sc::kCloudAccountId, "test-project"),
          OTelAttribute<std::string>(sc::kCloudPlatform,
                                     "gcp_kubernetes_engine"),
          OTelAttribute<std::string>(sc::kK8sClusterName, "test-cluster"),
          OTelAttribute<std::string>(sc::kHostId, "1020304050607080900"),
          OTelAttribute<std::string>(sc::kCloudRegion, "us-central1"),
      }));
}

TEST(ResourceDetector, GkeZone) {
  testing_util::ScopedEnvironment env("KUBERNETES_SERVICE_HOST", "0.0.0.0");
  auto constexpr kPayload = R"json({
  "instance": {
    "attributes": {
      "cluster-name": "test-cluster",
      "cluster-location": "projects/1234567890/zones/us-central1-a"
    },
    "id": 1020304050607080900
  },
  "project": {
    "projectId": "test-project"
  }
})json";

  auto detector = MakeTestDetector(kPayload);
  auto resource = detector->Detect();
  auto const& attributes = resource.GetAttributes();

  EXPECT_THAT(
      attributes,
      IsSupersetOf({
          OTelAttribute<std::string>(sc::kCloudProvider, "gcp"),
          OTelAttribute<std::string>(sc::kCloudAccountId, "test-project"),
          OTelAttribute<std::string>(sc::kCloudPlatform,
                                     "gcp_kubernetes_engine"),
          OTelAttribute<std::string>(sc::kK8sClusterName, "test-cluster"),
          OTelAttribute<std::string>(sc::kHostId, "1020304050607080900"),
          OTelAttribute<std::string>(sc::kCloudAvailabilityZone,
                                     "us-central1-a"),
      }));
}

TEST(ResourceDetector, CloudFunctions) {
  testing_util::ScopedEnvironment e1("KUBERNETES_SERVICE_HOST", absl::nullopt);
  testing_util::ScopedEnvironment e2("FUNCTION_TARGET", "set");
  testing_util::ScopedEnvironment e3("K_SERVICE", "test-service");
  testing_util::ScopedEnvironment e4("K_REVISION", "test-version");
  auto constexpr kPayload = R"json({
  "instance": {
    "id": 1020304050607080900
  },
  "project": {
    "projectId": "test-project"
  }
})json";

  auto detector = MakeTestDetector(kPayload);
  auto resource = detector->Detect();
  auto const& attributes = resource.GetAttributes();

  EXPECT_THAT(
      attributes,
      IsSupersetOf({
          OTelAttribute<std::string>(sc::kCloudProvider, "gcp"),
          OTelAttribute<std::string>(sc::kCloudAccountId, "test-project"),
          OTelAttribute<std::string>(sc::kCloudPlatform, "gcp_cloud_functions"),
          OTelAttribute<std::string>(sc::kFaasName, "test-service"),
          OTelAttribute<std::string>(sc::kFaasVersion, "test-version"),
          OTelAttribute<std::string>(sc::kFaasInstance, "1020304050607080900"),
      }));
}

TEST(ResourceDetector, CloudRun) {
  testing_util::ScopedEnvironment e1("KUBERNETES_SERVICE_HOST", absl::nullopt);
  testing_util::ScopedEnvironment e2("FUNCTION_TARGET", absl::nullopt);
  testing_util::ScopedEnvironment e3("K_CONFIGURATION", "set");
  testing_util::ScopedEnvironment e4("K_SERVICE", "test-service");
  testing_util::ScopedEnvironment e5("K_REVISION", "test-version");
  auto constexpr kPayload = R"json({
  "instance": {
    "id": 1020304050607080900
  },
  "project": {
    "projectId": "test-project"
  }
})json";

  auto detector = MakeTestDetector(kPayload);
  auto resource = detector->Detect();
  auto const& attributes = resource.GetAttributes();

  EXPECT_THAT(
      attributes,
      IsSupersetOf({
          OTelAttribute<std::string>(sc::kCloudProvider, "gcp"),
          OTelAttribute<std::string>(sc::kCloudAccountId, "test-project"),
          OTelAttribute<std::string>(sc::kCloudPlatform, "gcp_cloud_run"),
          OTelAttribute<std::string>(sc::kFaasName, "test-service"),
          OTelAttribute<std::string>(sc::kFaasVersion, "test-version"),
          OTelAttribute<std::string>(sc::kFaasInstance, "1020304050607080900"),
      }));
}

TEST(ResourceDetector, Gae) {
  testing_util::ScopedEnvironment e1("KUBERNETES_SERVICE_HOST", absl::nullopt);
  testing_util::ScopedEnvironment e2("FUNCTION_TARGET", absl::nullopt);
  testing_util::ScopedEnvironment e3("K_CONFIGURATION", absl::nullopt);
  testing_util::ScopedEnvironment e4("GAE_SERVICE", "test-service");
  testing_util::ScopedEnvironment e5("GAE_VERSION", "test-version");
  testing_util::ScopedEnvironment e6("GAE_INSTANCE", "test-instance");
  auto constexpr kPayload = R"json({
  "instance": {
    "zone": "projects/1234567890/zones/us-central1-a"
  },
  "project": {
    "projectId": "test-project"
  }
})json";

  auto detector = MakeTestDetector(kPayload);
  auto resource = detector->Detect();
  auto const& attributes = resource.GetAttributes();

  EXPECT_THAT(
      attributes,
      IsSupersetOf({
          OTelAttribute<std::string>(sc::kCloudProvider, "gcp"),
          OTelAttribute<std::string>(sc::kCloudAccountId, "test-project"),
          OTelAttribute<std::string>(sc::kCloudPlatform, "gcp_app_engine"),
          OTelAttribute<std::string>(sc::kFaasName, "test-service"),
          OTelAttribute<std::string>(sc::kFaasVersion, "test-version"),
          OTelAttribute<std::string>(sc::kFaasInstance, "test-instance"),
          OTelAttribute<std::string>(sc::kCloudAvailabilityZone,
                                     "us-central1-a"),
          OTelAttribute<std::string>(sc::kCloudRegion, "us-central1"),
      }));
}

TEST(ResourceDetector, Gce) {
  testing_util::ScopedEnvironment e1("KUBERNETES_SERVICE_HOST", absl::nullopt);
  testing_util::ScopedEnvironment e2("FUNCTION_TARGET", absl::nullopt);
  testing_util::ScopedEnvironment e3("K_CONFIGURATION", absl::nullopt);
  testing_util::ScopedEnvironment e4("GAE_SERVICE", absl::nullopt);
  auto constexpr kPayload = R"json({
  "instance": {
    "id": 1020304050607080900,
    "machineType": "projects/1234567890/machineTypes/c2d-standard-16",
    "name": "test-instance",
    "zone": "projects/1234567890/zones/us-central1-a"
  },
  "project": {
    "projectId": "test-project"
  }
})json";

  auto detector = MakeTestDetector(kPayload);
  auto resource = detector->Detect();
  auto const& attributes = resource.GetAttributes();

  EXPECT_THAT(
      attributes,
      IsSupersetOf({
          OTelAttribute<std::string>(sc::kCloudProvider, "gcp"),
          OTelAttribute<std::string>(sc::kCloudAccountId, "test-project"),
          OTelAttribute<std::string>(sc::kCloudPlatform, "gcp_compute_engine"),
          OTelAttribute<std::string>(sc::kHostType, "c2d-standard-16"),
          OTelAttribute<std::string>(sc::kHostId, "1020304050607080900"),
          OTelAttribute<std::string>(sc::kHostName, "test-instance"),
          OTelAttribute<std::string>(sc::kCloudAvailabilityZone,
                                     "us-central1-a"),
          OTelAttribute<std::string>(sc::kCloudRegion, "us-central1"),
      }));
}

TEST(ResourceDetector, CachesAttributes) {
  testing_util::ScopedEnvironment e1("KUBERNETES_SERVICE_HOST", absl::nullopt);
  testing_util::ScopedEnvironment e2("FUNCTION_TARGET", absl::nullopt);
  testing_util::ScopedEnvironment e3("K_CONFIGURATION", absl::nullopt);
  testing_util::ScopedEnvironment e4("GAE_SERVICE", absl::nullopt);
  auto constexpr kPayload = R"json({
  "instance": {
    "id": 1020304050607080900,
    "machineType": "projects/1234567890/machineTypes/c2d-standard-16",
    "name": "test-instance",
    "zone": "projects/1234567890/zones/us-central1-a"
  },
  "project": {
    "projectId": "test-project"
  }
})json";

  // Note that this detector expects exactly one RestClient::Get() call.
  auto detector = MakeTestDetector(kPayload);
  (void)detector->Detect();
  (void)detector->Detect();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
