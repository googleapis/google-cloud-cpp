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
#include "google/cloud/status.h"
#include <google/bigtable/v2/response_params.pb.h>
#include <grpcpp/grpcpp.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <memory>
#include <string>
#include <unordered_map>

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

struct DataLabels {
  std::string method;
  std::string streaming;
  std::string client_name;
  std::string client_uid;
  std::string app_profile;
  std::string status;
};

using LabelMap = std::unordered_map<std::string, std::string>;
LabelMap IntoLabelMap(ResourceLabels const& r, DataLabels const& d,
                      std::set<std::string> const& filtered_data_labels = {});

bool HasServerTiming(grpc::ClientContext const& client_context);
absl::optional<google::bigtable::v2::ResponseParams>
GetResponseParamsFromTrailingMetadata(
    grpc::ClientContext const& client_context);

absl::optional<double> GetServerLatencyFromInitialMetadata(
    grpc::ClientContext const& client_context);

struct PreCallParams {
  OperationContext::Clock::time_point attempt_start;
  bool first_attempt;
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

class Metric {
 public:
  using LatencyDuration = std::chrono::duration<double, std::milli>;

  virtual ~Metric() = 0;
  virtual void PreCall(opentelemetry::context::Context const&,
                       PreCallParams const&) {}
  virtual void PostCall(opentelemetry::context::Context const&,
                        grpc::ClientContext const&, PostCallParams const&) {}
  virtual void OnDone(opentelemetry::context::Context const&,
                      OnDoneParams const&) {}
  virtual void ElementRequest(opentelemetry::context::Context const&,
                              ElementRequestParams const&) {}
  virtual void ElementDelivery(opentelemetry::context::Context const&,
                               ElementDeliveryParams const&) {}
  virtual std::unique_ptr<Metric> clone(ResourceLabels resource_labels,
                                        DataLabels data_labels) const = 0;
};

class OperationLatency : public Metric {
 public:
  explicit OperationLatency(
      std::string const& instrumentation_scope,
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams const& p) override;
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  void OnDone(opentelemetry::context::Context const& context,
              OnDoneParams const& p) override;
  std::unique_ptr<Metric> clone(ResourceLabels resource_labels,
                                DataLabels data_labels) const override;

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      operation_latencies_;
  OperationContext::Clock::time_point operation_start_;
};

class AttemptLatency : public Metric {
 public:
  AttemptLatency(std::string const& instrumentation_scope,
                 opentelemetry::nostd::shared_ptr<
                     opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams const& p) override;
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  std::unique_ptr<Metric> clone(ResourceLabels resource_labels,
                                DataLabels data_labels) const override;

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      attempt_latencies_;
  OperationContext::Clock::time_point attempt_start_;
};

class RetryCount : public Metric {
 public:
  RetryCount(std::string const& instrumentation_scope,
             opentelemetry::nostd::shared_ptr<
                 opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams const&) override;
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  void OnDone(opentelemetry::context::Context const& context,
              OnDoneParams const& p) override;
  std::unique_ptr<Metric> clone(ResourceLabels resource_labels,
                                DataLabels data_labels) const override;

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::uint64_t num_retries_ = 0;
  opentelemetry::nostd::shared_ptr<
      opentelemetry::metrics::Counter<std::uint64_t>>
      retry_count_;
};

class FirstResponseLatency : public Metric {
 public:
  FirstResponseLatency(
      std::string const& instrumentation_scope,
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams const& p) override;
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  void ElementDelivery(opentelemetry::context::Context const&,
                       ElementDeliveryParams const&) override;
  void OnDone(opentelemetry::context::Context const& context,
              OnDoneParams const& p) override;

  std::unique_ptr<Metric> clone(ResourceLabels resource_labels,
                                DataLabels data_labels) const override;

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      first_response_latencies_;
  OperationContext::Clock::time_point operation_start_;
  absl::optional<LatencyDuration> first_response_latency_;
};

class ServerLatency : public Metric {
 public:
  ServerLatency(std::string const& instrumentation_scope,
                opentelemetry::nostd::shared_ptr<
                    opentelemetry::metrics::MeterProvider> const& provider);
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;

  std::unique_ptr<Metric> clone(ResourceLabels resource_labels,
                                DataLabels data_labels) const override;

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      server_latencies_;
};

class ConnectivityErrorCount : public Metric {
 public:
  ConnectivityErrorCount(
      std::string const& instrumentation_scope,
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider);
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  std::unique_ptr<Metric> clone(ResourceLabels resource_labels,
                                DataLabels data_labels) const override;

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::uint64_t num_errors_ = 0;
  opentelemetry::nostd::shared_ptr<
      opentelemetry::metrics::Counter<std::uint64_t>>
      connectivity_error_count_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_METRICS_H
