// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_METRICS_H

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include <grpcpp/grpcpp.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct ResourceLabels {
  std::string project_id;
  std::string instance;
  std::string table;
  std::string cluster;
  std::string zone;
};

std::ostream& operator<<(std::ostream& os, ResourceLabels const& r);

struct DataLabels {
  std::string method;
  std::string streaming;
  std::string client_name;
  std::string client_uid;
  std::string app_profile;
  std::string status;
};

using LabelMap = std::map<std::string, std::string>;
LabelMap IntoMap(ResourceLabels const& r, DataLabels const& d);
std::ostream& operator<<(std::ostream& os, LabelMap const& m);

struct PreCallParams {
  OperationContext::Clock::time_point attempt_start;
};

struct PostCallParams {
  OperationContext::Clock::time_point attempt_end;
  google::cloud::Status attempt_status;
};

struct OnDoneParams {
  OperationContext::Clock::time_point operation_end;
  google::cloud::Status operation_status;
};

struct ElementRequestParams {
  OperationContext::Clock::time_point element_request;
};

struct ElementDeliveryParams {
  OperationContext::Clock::time_point element_delivery;
  bool first_response;
};

// TODO: Determine whether "Params" should be const& or by-value.
// TODO: Consider moving Metric class to separate header as it is independent of
// Bigtable.
class Metric {
 public:
  virtual ~Metric() = 0;
  virtual void PreCall(opentelemetry::context::Context const&, PreCallParams) {}
  virtual void PostCall(
      opentelemetry::context::Context const&,
      std::multimap<grpc::string_ref, grpc::string_ref> const&,
      std::multimap<grpc::string_ref, grpc::string_ref> const&,
      PostCallParams) {}
  virtual void OnDone(opentelemetry::context::Context const&, OnDoneParams) {}
  virtual void ElementRequest(opentelemetry::context::Context const&,
                              ElementRequestParams) {}
  virtual void ElementDelivery(opentelemetry::context::Context const&,
                               ElementDeliveryParams) {}
  virtual std::unique_ptr<Metric> clone() const = 0;
};

class AttemptLatency : public Metric {
 public:
  AttemptLatency(
      ResourceLabels resource_labels, DataLabels data_labels,
      std::string const& name, std::string const& version,
      std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams p) override;
  void PostCall(
      opentelemetry::context::Context const& context,
      std::multimap<grpc::string_ref, grpc::string_ref> const& initial_metadata,
      std::multimap<grpc::string_ref, grpc::string_ref> const&
          trailing_metadata,
      PostCallParams p) override;
  std::unique_ptr<Metric> clone() const override;

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::shared_ptr<opentelemetry::metrics::Histogram<double>> attempt_latencies_;
  OperationContext::Clock::time_point attempt_start_;
};

class OperationLatency : public Metric {
 public:
  OperationLatency(
      ResourceLabels resource_labels, DataLabels data_labels,
      std::string const& name, std::string const& version,
      std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams p) override;
  void PostCall(
      opentelemetry::context::Context const& context,
      std::multimap<grpc::string_ref, grpc::string_ref> const& initial_metadata,
      std::multimap<grpc::string_ref, grpc::string_ref> const&
          trailing_metadata,
      PostCallParams p) override;

  void OnDone(opentelemetry::context::Context const& context,
              OnDoneParams p) override;
  std::unique_ptr<Metric> clone() const override;

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::shared_ptr<opentelemetry::metrics::Histogram<double>>
      operation_latencies_;
  OperationContext::Clock::time_point operation_start_;
};

class RetryCount : public Metric {
 public:
  RetryCount(
      ResourceLabels resource_labels, DataLabels data_labels,
      std::string const& name, std::string const& version,
      std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&, PreCallParams) override;
  void PostCall(
      opentelemetry::context::Context const& context,
      std::multimap<grpc::string_ref, grpc::string_ref> const& initial_metadata,
      std::multimap<grpc::string_ref, grpc::string_ref> const&
          trailing_metadata,
      PostCallParams p) override;
  std::unique_ptr<Metric> clone() const override;

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::shared_ptr<opentelemetry::metrics::Counter<std::uint64_t>> retry_count_;
};

class FirstResponseLatency : public Metric {
 public:
  FirstResponseLatency(
      ResourceLabels resource_labels, DataLabels data_labels,
      std::string const& name, std::string const& version,
      std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams p) override;
  void PostCall(
      opentelemetry::context::Context const& context,
      std::multimap<grpc::string_ref, grpc::string_ref> const& initial_metadata,
      std::multimap<grpc::string_ref, grpc::string_ref> const&
          trailing_metadata,
      PostCallParams p) override;
  void ElementDelivery(opentelemetry::context::Context const& context,
                       ElementDeliveryParams p) override;
  std::unique_ptr<Metric> clone() const override;

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::shared_ptr<opentelemetry::metrics::Histogram<double>>
      first_response_latencies_;
  OperationContext::Clock::time_point operation_start_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_METRICS_H
