// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_CI_OTEL_COLLECTOR_OTEL_COLLECTOR_H
#define GOOGLE_CLOUD_CPP_CI_OTEL_COLLECTOR_OTEL_COLLECTOR_H

#include "google/cloud/version.h"
#include "protos/google/cloud/opentelemetry/testing/observability_verification.grpc.pb.h"
#include <google/monitoring/v3/metric_service.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

/**
 * In-process or standalone test server that intercepts Google Cloud
 * Observability RPCs (e.g. Cloud Monitoring MetricService) and allows
 * integration test harnesses to inspect recorded request protobufs over gRPC.
 */
class OtelCollectorServer final
    : public google::monitoring::v3::MetricService::Service,
      public google::cloud::opentelemetry::testing::
          ObservabilityVerificationService::Service {
 public:
  OtelCollectorServer() = default;

  // --- google.monitoring.v3.MetricService Implementation ---
  grpc::Status CreateTimeSeries(
      grpc::ServerContext* context,
      google::monitoring::v3::CreateTimeSeriesRequest const* request,
      google::protobuf::Empty* response) override;

  grpc::Status CreateServiceTimeSeries(
      grpc::ServerContext* context,
      google::monitoring::v3::CreateTimeSeriesRequest const* request,
      google::protobuf::Empty* response) override;

  // --- google.cloud.opentelemetry.testing.ObservabilityVerificationService
  // Implementation ---
  grpc::Status GetRecordedMetrics(
      grpc::ServerContext* context, google::protobuf::Empty const* request,
      google::cloud::opentelemetry::testing::GetRecordedMetricsResponse*
          response) override;

  grpc::Status ClearRecordedMetrics(grpc::ServerContext* context,
                                    google::protobuf::Empty const* request,
                                    google::protobuf::Empty* response) override;

  // Direct C++ accessor for in-process testing without gRPC client overhead
  std::vector<google::monitoring::v3::CreateTimeSeriesRequest>
  recorded_metrics() const;

  void Clear();

 private:
  mutable std::mutex mu_;
  std::vector<google::monitoring::v3::CreateTimeSeriesRequest> metric_requests_;
};

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_CI_OTEL_COLLECTOR_OTEL_COLLECTOR_H
