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

#include "ci/otel_collector/otel_collector.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

grpc::Status OtelCollectorServer::CreateTimeSeries(
    grpc::ServerContext* /*context*/,
    google::monitoring::v3::CreateTimeSeriesRequest const* request,
    google::protobuf::Empty* /*response*/) {
  std::lock_guard<std::mutex> lock(mu_);
  metric_requests_.push_back(*request);
  return grpc::Status::OK;
}

grpc::Status OtelCollectorServer::CreateServiceTimeSeries(
    grpc::ServerContext* /*context*/,
    google::monitoring::v3::CreateTimeSeriesRequest const* request,
    google::protobuf::Empty* /*response*/) {
  std::lock_guard<std::mutex> lock(mu_);
  metric_requests_.push_back(*request);
  return grpc::Status::OK;
}

grpc::Status OtelCollectorServer::GetRecordedMetrics(
    grpc::ServerContext* /*context*/,
    google::protobuf::Empty const* /*request*/,
    google::cloud::opentelemetry::testing::GetRecordedMetricsResponse*
        response) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto const& req : metric_requests_) {
    *response->add_requests() = req;
  }
  return grpc::Status::OK;
}

grpc::Status OtelCollectorServer::ClearRecordedMetrics(
    grpc::ServerContext* /*context*/,
    google::protobuf::Empty const* /*request*/,
    google::protobuf::Empty* /*response*/) {
  Clear();
  return grpc::Status::OK;
}

std::vector<google::monitoring::v3::CreateTimeSeriesRequest>
OtelCollectorServer::recorded_metrics() const {
  std::lock_guard<std::mutex> lock(mu_);
  return metric_requests_;
}

void OtelCollectorServer::Clear() {
  std::lock_guard<std::mutex> lock(mu_);
  metric_requests_.clear();
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
